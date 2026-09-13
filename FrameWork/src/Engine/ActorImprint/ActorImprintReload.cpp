#include "ActorImprintSystem.h"
#include "Engine/ActorImprint/ActorImprintInstanceRegistry.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Scene/SceneWriter.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace
{
	constexpr std::size_t NoSceneIndex = static_cast<std::size_t>(-1);

	struct ReloadStateGuard
	{
		bool& reloading;
		bool& buildingCandidate;
		ActorImprintHandle& candidateHandle;
		const ActorImprint*& candidateDefinition;

		ReloadStateGuard(bool& reloadFlag, bool& buildFlag,
			ActorImprintHandle& handle, const ActorImprint*& definition)
			: reloading(reloadFlag), buildingCandidate(buildFlag),
			candidateHandle(handle), candidateDefinition(definition)
		{
			reloading = true;
		}

		~ReloadStateGuard()
		{
			buildingCandidate = false;
			candidateHandle = {};
			candidateDefinition = nullptr;
			reloading = false;
		}
	};
}

ActorImprintReloadResult::~ActorImprintReloadResult()
{
	FinalizeRetiredScenes();
}

ActorImprintReloadResult::ActorImprintReloadResult(ActorImprintReloadResult&& other) noexcept = default;

void ActorImprintReloadResult::FinalizeRetiredScenes() noexcept
{
	for (auto& scene : m_retiredScenes)
	{
		if (scene) scene->Finalize();
	}
	m_retiredScenes.clear();
	m_retiredDefinition.reset();
}

ActorImprintReloadResult ActorImprintSystem::Reload(const AssetChange& change,
	std::span<std::unique_ptr<SceneBase>* const> liveScenes)
{
	ActorImprintReloadResult result;
	result.assetGuid = change.guid;
	const auto Fail = [&](ActorImprintReloadErrorCode code, std::string message,
		std::size_t sceneIndex = NoSceneIndex, std::string path = {})
	{
		result.status = ActorImprintReloadStatus::Failed;
		result.error = { code, sceneIndex, std::move(path), std::move(message) };
	};

	if (!change.guid.IsValid())
	{
		Fail(ActorImprintReloadErrorCode::InvalidChange,
			"Asset change requires a nonzero Asset GUID.");
		return result;
	}

	const ActorImprintHandle handle = FindHandle(change.guid);
	if (handle.IsNull())
	{
		result.status = ActorImprintReloadStatus::Ignored;
		return result;
	}
	if (m_materializing || m_reloading)
	{
		Fail(ActorImprintReloadErrorCode::TransactionInProgress,
			"Another ActorImprint materialization or reload transaction is active.");
		return result;
	}

	Slot& slot = m_slots[handle.index];
	if (slot.generation != handle.generation || !slot.definition || slot.assetGuid != change.guid)
	{
		Fail(ActorImprintReloadErrorCode::InvalidChange,
			"Loaded ActorImprint identity is inconsistent with the Asset change.");
		return result;
	}
	result.previousRevision = slot.definition->GetRevision();
	result.currentRevision = result.previousRevision;

	// Deferred dispatch may observe Removed (or a type loss) followed by a
	// same-GUID catalog restore before either event is consumed. Latch that
	// identity loss now; only a fully validated candidate below may clear it.
	if (change.kind == AssetChangeKind::Removed || change.type != AssetType::ActorImprint)
		slot.missing = true;

	// The catalog is the final observation for a coalesced batch. A removal or
	// type change retains the loaded definition and every live Instance as Missing.
	const AssetEntry* catalogEntry = m_assets.GetAssetEntry(change.guid);
	if (!catalogEntry || catalogEntry->type != AssetType::ActorImprint)
	{
		slot.missing = true;
		result.status = ActorImprintReloadStatus::Missing;
		return result;
	}

	AssetManagerAssetReferenceContext assetContext(m_assets);
	ActorImprintAssetError assetError;
	auto candidateDefinition = ActorImprintAssetDeserializer::Load(
		m_assets.GetAssetPath(change.guid), &assetContext, &assetError);
	if (!candidateDefinition)
	{
		Fail(ActorImprintReloadErrorCode::CandidateAssetFailed,
			assetError.message.empty() ? "ActorImprint candidate definition failed validation." : assetError.message,
			NoSceneIndex, std::move(assetError.path));
		return result;
	}

	result.currentRevision = candidateDefinition->GetRevision();
	if (result.currentRevision == result.previousRevision)
	{
		// A same-GUID restore is not available until the file has passed the full
		// deserializer, even when its semantic Revision is unchanged.
		slot.missing = false;
		result.status = ActorImprintReloadStatus::NoChange;
		return result;
	}

	ReloadStateGuard transaction(m_reloading, m_buildingReloadCandidate,
		m_reloadHandle, m_reloadDefinition);

	struct StagedScene
	{
		std::size_t inputIndex = NoSceneIndex;
		std::unique_ptr<SceneBase>* owner = nullptr;
		nlohmann::json snapshot;
		Vector2 viewport = Vector2::One();
		std::unique_ptr<SceneBase> candidate;
	};

	std::vector<StagedScene> stagedScenes;
	try
	{
		stagedScenes.reserve(liveScenes.size());
		result.affectedSceneIndices.reserve(liveScenes.size());
		std::unordered_set<const void*> uniqueOwners;
		uniqueOwners.reserve(liveScenes.size());
		std::size_t suppliedInstanceCount = 0;

		for (std::size_t sceneIndex = 0; sceneIndex < liveScenes.size(); ++sceneIndex)
		{
			std::unique_ptr<SceneBase>* owner = liveScenes[sceneIndex];
			if (!owner || !*owner || !uniqueOwners.insert(owner).second)
			{
				Fail(ActorImprintReloadErrorCode::InvalidSceneSet,
					"Live Scene owner entries must be unique and non-null.", sceneIndex);
				return result;
			}
			SceneBase& scene = **owner;
			if (scene.m_isFinalized || scene.m_actorBatchActive || scene.m_unpublishedCandidate)
			{
				Fail(ActorImprintReloadErrorCode::InvalidSceneSet,
					"Live Scene must be a published, idle, non-finalized Scene.", sceneIndex);
				return result;
			}
			EngineContext* context = scene.GetEngineContext();
			if (!context || context->pActorImprintSystem != this || context->pAssetManager != &m_assets)
			{
				Fail(ActorImprintReloadErrorCode::InvalidSceneSet,
					"Live Scene does not use this ActorImprintSystem and AssetManager.", sceneIndex);
				return result;
			}

			std::size_t sceneInstanceCount = 0;
			for (const auto& [root, record] : scene.GetImprintInstances().GetInstances())
			{
				(void)root;
				if (record.imprint != handle && record.assetGuid != change.guid) continue;
				if (record.imprint != handle || record.assetGuid != change.guid)
				{
					Fail(ActorImprintReloadErrorCode::InvalidSceneSet,
						"Live Scene contains inconsistent ActorImprint provenance.", sceneIndex);
					return result;
				}
				++sceneInstanceCount;
			}
			if (sceneInstanceCount == 0) continue;

			suppliedInstanceCount += sceneInstanceCount;
			StagedScene staged;
			staged.inputIndex = sceneIndex;
			staged.owner = owner;
			staged.viewport = scene.GetViewportSize();
			if (!SceneWriter::SerializeReloadSnapshot(&scene, staged.snapshot))
			{
				Fail(ActorImprintReloadErrorCode::SceneSnapshotFailed,
					"Could not capture the live Scene before ActorImprint reload.", sceneIndex);
				return result;
			}
			result.affectedSceneIndices.push_back(sceneIndex);
			stagedScenes.push_back(std::move(staged));
		}

		if (suppliedInstanceCount != slot.instances)
		{
			Fail(ActorImprintReloadErrorCode::IncompleteLiveSceneSet,
				"The supplied Scene set does not contain every live Instance of this ActorImprint.");
			return result;
		}

		// Only the private, callback-free candidate construction phase observes the
		// new definition. Public Resolve continues to expose the old Slot otherwise.
		m_buildingReloadCandidate = true;
		m_reloadHandle = handle;
		m_reloadDefinition = candidateDefinition.get();

		for (StagedScene& staged : stagedScenes)
		{
			SceneLoadResult load = SceneLoader::LoadPreparedCandidate(
				staged.snapshot, *(*staged.owner)->GetEngineContext(), "<actor-imprint-reload>");
			if (!load)
			{
				Fail(ActorImprintReloadErrorCode::SceneCandidateFailed,
					load.error.message.empty() ? "ActorImprint reload Scene candidate failed." : load.error.message,
					staged.inputIndex, std::move(load.error.path));
				return result;
			}
			staged.candidate = std::move(load.scene);
			const double viewportWidth = static_cast<double>(staged.viewport.x);
			const double viewportHeight = static_cast<double>(staged.viewport.y);
			const double maximumViewport = static_cast<double>(std::numeric_limits<UINT>::max());
			if (std::isfinite(viewportWidth) && std::isfinite(viewportHeight) &&
				viewportWidth > 0.0 && viewportHeight > 0.0 &&
				viewportWidth <= maximumViewport && viewportHeight <= maximumViewport)
			{
				staged.candidate->SetViewportSize(
					static_cast<UINT>(viewportWidth), static_cast<UINT>(viewportHeight));
			}
		}

		// Reserve every result container before the non-failing commit starts.
		result.m_retiredScenes.reserve(stagedScenes.size());
	}
	catch (const std::exception& exception)
	{
		Fail(ActorImprintReloadErrorCode::SceneCandidateFailed, exception.what());
		return result;
	}
	catch (...)
	{
		Fail(ActorImprintReloadErrorCode::SceneCandidateFailed,
			"ActorImprint reload candidate construction threw an unknown exception.");
		return result;
	}

	// No callback or fallible operation has occurred. From here, unique_ptr moves
	// and the approved non-failing attachment lifecycle form the commit boundary.
	m_buildingReloadCandidate = false;
	m_reloadHandle = {};
	m_reloadDefinition = nullptr;
	result.m_retiredDefinition = std::move(slot.definition);
	slot.definition = std::move(candidateDefinition);
	slot.missing = false;

	for (StagedScene& staged : stagedScenes)
	{
		result.m_retiredScenes.push_back(std::move(*staged.owner));
		*staged.owner = std::move(staged.candidate);
	}
	for (StagedScene& staged : stagedScenes)
	{
		if (*staged.owner) (*staged.owner)->PublishUnpublishedCandidate();
	}

	result.status = ActorImprintReloadStatus::Reloaded;
	return result;
}
