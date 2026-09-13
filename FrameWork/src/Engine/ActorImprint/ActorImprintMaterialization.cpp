#include "ActorImprintSystem.h"
#include "ActorImprintInstanceRegistry.h"
#include "ActorImprintPropertyOverrides.h"
#include "Detail/ActorImprintDetachedActors.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/SceneActorBatch.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include <unordered_set>

namespace
{
	bool PrepareRestoredActorGuids(const ActorImprint& definition, const SceneBase& destination,
		const ActorImprintRestoreInput& input, ActorImprintReferenceCodec::ActorGuids& outGuids,
		const std::unordered_set<Guid>* reservedSceneGuids, std::string& outError)
	{
		if (!input.sourceDefinitionRevision.IsValid())
		{
			outError = "Restored Instance requires a valid source DefinitionRevision.";
			return false;
		}
		if (!input.rootActorGuid.IsValid())
		{
			outError = "Restored Instance requires a nonzero root Actor GUID.";
			return false;
		}

		std::unordered_set<Guid> usedGuids;
		usedGuids.reserve(input.actorGuids.size() + definition.GetActors().size());
		for (const auto& [id, guid] : input.actorGuids)
		{
			if (id == InvalidLocalObjectId || !guid.IsValid() || !usedGuids.insert(guid).second)
			{
				outError = "Saved Actor GUID mappings require unique nonzero LocalObjectIDs and GUIDs.";
				return false;
			}
		}

		const bool sameRevision = input.sourceDefinitionRevision == definition.GetRevision();
		if (sameRevision && input.actorGuids.size() != definition.GetActors().size())
		{
			outError = "Same-revision Actor GUID mappings must cover exactly the current definition.";
			return false;
		}
		const auto root = input.actorGuids.find(definition.GetRootActorId());
		if (root == input.actorGuids.end() || root->second != input.rootActorGuid)
		{
			outError = "The saved root Actor GUID does not match the current root LocalObjectID mapping.";
			return false;
		}

		ActorImprintReferenceCodec::ActorGuids migrated;
		migrated.reserve(definition.GetActors().size());
		for (const auto& actor : definition.GetActors())
		{
			const auto saved = input.actorGuids.find(actor.id);
			if (saved != input.actorGuids.end())
			{
				migrated.emplace(actor.id, saved->second);
				continue;
			}
			if (sameRevision)
			{
				outError = "Same-revision Actor GUID mapping is incomplete.";
				return false;
			}
			Guid generated;
			do generated = GuidGenerator::Generate();
			while (!generated.IsValid() || usedGuids.contains(generated) ||
				(reservedSceneGuids && reservedSceneGuids->contains(generated)) ||
				!destination.FindActorHandle(generated).IsNull());
			usedGuids.insert(generated);
			migrated.emplace(actor.id, generated);
		}
		outGuids = std::move(migrated);
		return true;
	}
}

bool ActorImprintSystem::DestroyInstance(SceneBase& scene, ActorHandle root, StructuralMutationResult* result)
{
	if (m_materializing || m_reloading || scene.m_actorBatchActive)
		return StructuralMutationResult{ StructuralMutationReason::TransactionInProgress }.Report(result);
	if (scene.m_imprintInstances.m_system != this)
		return StructuralMutationResult{ StructuralMutationReason::InvalidInstance }.Report(result);
	if (!scene.ValidateInstanceDestruction(root).Report(result)) return false;
	CommitDestroyInstance(scene, root);
	return true;
}

void ActorImprintSystem::CommitDestroyInstance(SceneBase& scene, ActorHandle root) noexcept
{
	scene.CommitInstanceDestruction(root);
}

Actor* ActorImprintSystem::Instantiate(SceneBase& scene, ActorImprintHandle imprint,
	ActorHandle externalParent, ActorImprintMaterializationError* outError)
{
	return Materialize(scene, imprint, externalParent, nullptr, nullptr, outError);
}

Actor* ActorImprintSystem::Instantiate(SceneBase& scene, const AssetReference<ActorImprint>& imprint,
	ActorHandle externalParent, ActorImprintMaterializationError* outError)
{
	ActorImprintLoadError loadError;
	const auto handle = Load(imprint, &loadError);
	if (handle.IsNull())
	{
		if (outError) *outError = { ActorImprintMaterializationErrorCode::InvalidAsset, 0, {}, loadError.message };
		return nullptr;
	}
	return Materialize(scene, handle, externalParent, nullptr, nullptr, outError);
}

Actor* ActorImprintSystem::RestoreInstance(SceneBase& scene, ActorImprintHandle imprint,
	const ActorImprintRestoreInput& input, ActorImprintMaterializationError* outError)
{
	return Materialize(scene, imprint, input.externalParent, &input, nullptr, outError);
}

Actor* ActorImprintSystem::RestoreInstanceForSceneCandidate(SceneBase& scene,
	ActorImprintHandle imprint, const ActorImprintRestoreInput& input,
	const std::unordered_set<Guid>& reservedSceneGuids,
	ActorImprintMaterializationError* outError)
{
	return Materialize(scene, imprint, input.externalParent, &input, &reservedSceneGuids, outError);
}

Actor* ActorImprintSystem::Materialize(SceneBase& scene, ActorImprintHandle imprint, ActorHandle parentHandle,
	const ActorImprintRestoreInput* restoreInput, const std::unordered_set<Guid>* reservedSceneGuids,
	ActorImprintMaterializationError* outError)
{
	if (outError) *outError = {};
	const auto fail = [&](ActorImprintMaterializationErrorCode code, std::string message,
		LocalObjectId id = 0, std::string path = {}) -> Actor* {
		if (outError) *outError = { code, id, std::move(path), std::move(message) };
		return nullptr;
	};
	const auto* context = scene.GetEngineContext();
	const bool buildingReloadCandidate = m_reloading && m_buildingReloadCandidate;
	if (m_materializing || (m_reloading && !buildingReloadCandidate) ||
		scene.m_actorBatchActive || scene.m_isFinalized || !context ||
		context->pActorImprintSystem != this || context->pAssetManager != &m_assets)
		return fail(ActorImprintMaterializationErrorCode::InvalidScene, "Scene must use this App's ActorImprintSystem and AssetManager.");
	const auto* definition = ResolveForSceneCandidate(imprint);
	AssetManagerAssetReferenceContext assetContext(m_assets);
	if (!definition || (!buildingReloadCandidate &&
		(GetAvailability(imprint) != ActorImprintAvailability::Available ||
		assetContext.Validate(GetAssetGuid(imprint), AssetType::ActorImprint) != AssetReferenceCodecResult::Success)))
		return fail(ActorImprintMaterializationErrorCode::InvalidAsset, "Imprint handle is invalid or its asset is missing.");
	Actor* parent = scene.ResolveActor(parentHandle);
	if (!parentHandle.IsNull() && (!parent || parent->IsDestroyed() || scene.m_imprintInstances.FindMember(parentHandle)))
		return fail(ActorImprintMaterializationErrorCode::InvalidParent, "External parent must be a live ordinary Actor in the destination Scene.");

	struct BusyScope
	{
		bool& flag;
		explicit BusyScope(bool& value) : flag(value) { flag = true; }
		~BusyScope() { flag = false; }
	} busy(m_materializing);
	try
	{
		ActorImprintReferenceCodec::ActorGuids restoredGuids;
		const ActorImprintReferenceCodec::ActorGuids* restoredGuidInput = nullptr;
		const std::vector<ActorImprintPropertyOverrideTarget>* overrides = nullptr;
		ActorImprintOverrideRevisionRelation revisionRelation = ActorImprintOverrideRevisionRelation::Same;
		if (restoreInput)
		{
			std::string identityError;
			if (!PrepareRestoredActorGuids(*definition, scene, *restoreInput, restoredGuids,
				reservedSceneGuids, identityError))
				return fail(ActorImprintMaterializationErrorCode::CandidateFailed, std::move(identityError));
			restoredGuidInput = &restoredGuids;
			overrides = &restoreInput->propertyOverrides;
			revisionRelation = restoreInput->sourceDefinitionRevision == definition->GetRevision()
				? ActorImprintOverrideRevisionRelation::Same : ActorImprintOverrideRevisionRelation::Different;
		}
		SceneActorBatch batch(scene);
		ActorImprintDetail::DetachedActorsError error;
		auto detached = ActorImprintDetail::CreateDetachedActors(*definition, scene, restoredGuidInput,
			overrides, revisionRelation, error);
		if (!detached) return fail(ActorImprintMaterializationErrorCode::CandidateFailed,
			std::move(error.message), error.objectId, std::move(error.path));
		batch.Stage(std::move(*detached));
		ActorImprintInstanceRecord record;
		record.assetGuid = GetAssetGuid(imprint);
		record.imprint = imprint;
		record.sourceRevision = definition->GetRevision();
		record.rootId = definition->GetRootActorId();
		for (std::size_t i = 0; i < definition->GetActors().size(); ++i)
		{
			const auto& actorDefinition = definition->GetActors()[i];
			const auto handle = batch.Handles()[i];
			Actor* actor = batch.Candidate().ResolveActor(handle);
			record.actors.emplace(actorDefinition.id, ActorImprintActorIdentity{ actor->GetGuid(), handle });
			if (actorDefinition.id == record.rootId) record.root = handle;
			std::unordered_map<std::string, std::size_t> occurrences;
			for (const auto& component : actorDefinition.components)
				record.components.emplace(component.id, ActorImprintComponentLocator{
					actorDefinition.id, component.typeName, occurrences[component.typeName]++ });
		}
		for (const auto& actor : definition->GetActors())
		{
			if (actor.parentId == 0) continue;
			if (!batch.SetParent(batch.Candidate().ResolveActor(record.actors.at(actor.id).handle),
				batch.Candidate().ResolveActor(record.actors.at(actor.parentId).handle)))
				return fail(ActorImprintMaterializationErrorCode::CandidateFailed, "Candidate hierarchy failed.", actor.id);
		}
		Actor* root = batch.Candidate().ResolveActor(record.root);
		std::string validationError;
		if (!scene.m_unpublishedCandidate && !batch.ResolveAndValidate(root, parent, validationError))
			return fail(ActorImprintMaterializationErrorCode::CandidateFailed, std::move(validationError));

		auto& registry = scene.m_imprintInstances;
		auto instances = registry.m_instances;
		auto members = registry.m_members;
		for (const auto& [id, actor] : record.actors)
			if (!members.emplace(actor.handle, ActorImprintMembership{ record.root, id }).second)
				return fail(ActorImprintMaterializationErrorCode::CandidateFailed, "Instance membership conflict.", id);
		if (!instances.emplace(record.root, std::move(record)).second)
			return fail(ActorImprintMaterializationErrorCode::CandidateFailed, "Instance root already exists.");
		batch.PrepareCommit(root, parent);
		// Recoverable validation and Actor/Registry storage preparation end here.
		// Attach callbacks are non-failing; allocation failure there is fatal.
		batch.Commit();
		registry.m_instances.swap(instances);
		registry.m_members.swap(members);
		registry.m_system = this;
		++m_slots[imprint.index].instances;
		batch.Attach();
		return root;
	}
	catch (const std::exception& error)
	{
		return fail(ActorImprintMaterializationErrorCode::CandidateFailed, error.what());
	}
	catch (...)
	{
		return fail(ActorImprintMaterializationErrorCode::CandidateFailed, "Candidate construction threw an unknown exception.");
	}
}
