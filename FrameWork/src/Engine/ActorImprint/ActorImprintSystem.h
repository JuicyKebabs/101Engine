#pragma once
#include "ActorImprintAssetDeserializer.h"
#include "ActorImprintHandle.h"
#include "ActorImprintInstanceRecord.h"
#include "ActorImprintReferenceCodec.h"
#include "Engine/Actor/ActorHandle.h"
#include "Engine/Scene/StructuralMutationResult.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Resource/AssetChange.h"
#include "Engine/Resource/AssetReference.h"
#include <cstddef>
#include <memory>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class AssetManager;
class SceneBase;
class Actor;

enum class ActorImprintAvailability
{
	Invalid,
	Available,
	Missing,
};

enum class ActorImprintMaterializationErrorCode { None, InvalidScene, InvalidAsset, InvalidParent, CandidateFailed };
struct ActorImprintMaterializationError
{
	ActorImprintMaterializationErrorCode code = ActorImprintMaterializationErrorCode::None;
	LocalObjectId objectId = 0;
	std::string path;
	std::string message;
};

struct ActorImprintRestoreInput
{
	DefinitionRevision sourceDefinitionRevision;
	Guid rootActorGuid;
	ActorImprintReferenceCodec::ActorGuids actorGuids;
	std::vector<ActorImprintPropertyOverrideTarget> propertyOverrides;
	ActorHandle externalParent;
};

enum class ActorImprintLoadErrorCode
{
	None,
	InvalidReference,
	InvalidAsset,
	Missing,
	PoolExhausted,
	TransactionInProgress,
};

struct ActorImprintLoadError
{
	ActorImprintLoadErrorCode code = ActorImprintLoadErrorCode::None;
	AssetReferenceCodecResult referenceResult = AssetReferenceCodecResult::Success;
	ActorImprintAssetError assetError;
	std::string message;
};

enum class ActorImprintReloadStatus
{
	Ignored,
	NoChange,
	Reloaded,
	Missing,
	Failed,
};

enum class ActorImprintReloadErrorCode
{
	None,
	InvalidChange,
	TransactionInProgress,
	InvalidSceneSet,
	IncompleteLiveSceneSet,
	CandidateAssetFailed,
	SceneSnapshotFailed,
	SceneCandidateFailed,
};

struct ActorImprintReloadError
{
	ActorImprintReloadErrorCode code = ActorImprintReloadErrorCode::None;
	std::size_t sceneIndex = static_cast<std::size_t>(-1);
	std::string path;
	std::string message;
};

// A successful Scene replacement keeps displaced Scenes and the old immutable
// definition alive until the caller has cleared pointer-bearing Editor history.
// Call FinalizeRetiredScenes immediately after updating that owner-specific state.
class ActorImprintReloadResult
{
public:
	ActorImprintReloadResult() = default;
	~ActorImprintReloadResult();
	ActorImprintReloadResult(ActorImprintReloadResult&& other) noexcept;
	ActorImprintReloadResult& operator=(ActorImprintReloadResult&&) = delete;
	ActorImprintReloadResult(const ActorImprintReloadResult&) = delete;
	ActorImprintReloadResult& operator=(const ActorImprintReloadResult&) = delete;

	ActorImprintReloadStatus status = ActorImprintReloadStatus::Ignored;
	Guid assetGuid;
	DefinitionRevision previousRevision;
	DefinitionRevision currentRevision;
	std::vector<std::size_t> affectedSceneIndices;
	ActorImprintReloadError error;

	explicit operator bool() const { return status != ActorImprintReloadStatus::Failed; }
	bool ReplacedScenes() const { return status == ActorImprintReloadStatus::Reloaded && !affectedSceneIndices.empty(); }
	void FinalizeRetiredScenes() noexcept;

private:
	friend class ActorImprintSystem;
	// Declaration order makes the retired Scenes die before their definition if
	// ordinary stack unwinding ever bypasses FinalizeRetiredScenes().
	std::unique_ptr<const ActorImprint> m_retiredDefinition;
	std::vector<std::unique_ptr<SceneBase>> m_retiredScenes;
};

// App-owned; AssetManager and registered Component modules must outlive use.
// Single-threaded, like the existing Scene and resource managers.
class ActorImprintSystem
{
public:
	explicit ActorImprintSystem(const AssetManager& assets) : m_assets(assets) {}
	ActorImprintSystem(const ActorImprintSystem&) = delete;
	ActorImprintSystem& operator=(const ActorImprintSystem&) = delete;

	ActorImprintHandle Load(const Guid& assetGuid, ActorImprintLoadError* outError = nullptr);
	ActorImprintHandle Load(const AssetReference<ActorImprint>& reference, ActorImprintLoadError* outError = nullptr)
	{
		return Load(reference.GetGuid(), outError);
	}
	ActorImprintHandle FindHandle(const Guid& assetGuid) const;
	const ActorImprint* Resolve(ActorImprintHandle handle) const;
	Guid GetAssetGuid(ActorImprintHandle handle) const;
	ActorImprintAvailability GetAvailability(ActorImprintHandle handle) const;
	ActorImprintAvailability GetAvailability(const Guid& assetGuid) const
	{
		return GetAvailability(FindHandle(assetGuid));
	}
	bool IsMissing(ActorImprintHandle handle) const
	{
		return GetAvailability(handle) == ActorImprintAvailability::Missing;
	}
	std::size_t GetLoadedCount() const { return m_handles.size(); }
	std::size_t GetLiveInstanceCount(const Guid& assetGuid) const;
	ActorImprintReloadResult Reload(const AssetChange& change,
		std::span<std::unique_ptr<SceneBase>* const> liveScenes);
	Actor* Instantiate(SceneBase& scene, ActorImprintHandle imprint,
		ActorHandle externalParent = {}, ActorImprintMaterializationError* outError = nullptr);
	Actor* Instantiate(SceneBase& scene, const AssetReference<ActorImprint>& imprint,
		ActorHandle externalParent = {}, ActorImprintMaterializationError* outError = nullptr);
	Actor* RestoreInstance(SceneBase& scene, ActorImprintHandle imprint,
		const ActorImprintRestoreInput& input, ActorImprintMaterializationError* outError = nullptr);
	bool DestroyInstance(SceneBase& scene, ActorHandle root, StructuralMutationResult* result = nullptr);

	// Borrowed definition pointers expire on successful Reload, Unload/Clear,
	// or System destruction. A successful Reload preserves the Handle identity.
	bool Unload(ActorImprintHandle handle);
	bool Clear();

private:
	friend class ActorImprintInstanceRegistry;
	friend class SceneBase;
	friend class SceneLoader;
	void CommitDestroyInstance(SceneBase& scene, ActorHandle root) noexcept;
	void ReleaseInstance(ActorImprintHandle handle);
	Actor* RestoreInstanceForSceneCandidate(SceneBase& scene, ActorImprintHandle imprint,
		const ActorImprintRestoreInput& input, const std::unordered_set<Guid>& reservedSceneGuids,
		ActorImprintMaterializationError* outError);
	Actor* Materialize(SceneBase& scene, ActorImprintHandle imprint, ActorHandle externalParent,
		const ActorImprintRestoreInput* restoreInput, const std::unordered_set<Guid>* reservedSceneGuids,
		ActorImprintMaterializationError* outError);
	const ActorImprint* ResolveForSceneCandidate(ActorImprintHandle handle) const;
	struct Slot
	{
		std::unique_ptr<const ActorImprint> definition;
		Guid assetGuid;
		std::uint32_t generation = 0;
		std::size_t instances = 0;
		bool missing = false;
	};
	const AssetManager& m_assets;
	std::vector<Slot> m_slots;
	std::vector<std::uint32_t> m_freeIndices;
	std::unordered_map<Guid, ActorImprintHandle> m_handles;
	bool m_materializing = false;
	bool m_reloading = false;
	bool m_buildingReloadCandidate = false;
	ActorImprintHandle m_reloadHandle;
	const ActorImprint* m_reloadDefinition = nullptr;
};
