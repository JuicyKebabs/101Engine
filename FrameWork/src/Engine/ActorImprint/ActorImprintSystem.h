#pragma once
#include "ActorImprintAssetDeserializer.h"
#include "ActorImprintHandle.h"
#include "ActorImprintInstanceRecord.h"
#include "ActorImprintReferenceCodec.h"
#include "Engine/Actor/ActorHandle.h"
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

struct ActorImprintRestoreInput
{
	DefinitionRevision sourceDefinitionRevision;
	Guid rootActorGuid;
	ActorImprintReferenceCodec::ActorGuids actorGuids;
	std::vector<ActorImprintPropertyOverrideTarget> propertyOverrides;
	ActorHandle externalParent;
};

enum class ActorImprintReloadStatus
{
	Ignored,
	NoChange,
	Reloaded,
	Missing,
	Failed,
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

	explicit operator bool() const
	{
		return status != ActorImprintReloadStatus::Failed;
	}
	bool ReplacedScenes() const
	{
		return status == ActorImprintReloadStatus::Reloaded && !affectedSceneIndices.empty();
	}
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

	// Load an ActorImprint asset into the system.
	// From .imprint file to
	ActorImprintHandle Load(const Guid& assetGuid);
	ActorImprintHandle Load(const AssetReference<ActorImprint>& reference)
	{
		return Load(reference.GetGuid());
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

	ActorImprintReloadResult Reload(const AssetChange& change,std::span<std::unique_ptr<SceneBase>* const> liveScenes);

	// Instantiate from a loaded ActorImprintHandle
	Actor* Instantiate(
		SceneBase& scene,
		ActorImprintHandle imprint,
		ActorHandle externalParent = {});

	// Instantiate from an assetReference
	Actor* Instantiate(
		SceneBase& scene,
		const AssetReference<ActorImprint>& imprint,
		ActorHandle externalParent = {});

	Actor* RestoreInstance(SceneBase& scene, ActorImprintHandle imprint,const ActorImprintRestoreInput& input);

	bool DestroyInstance(SceneBase& scene, ActorHandle root);

	// Borrowed definition pointers expire on successful Reload, Unload/Clear,
	// or System destruction. A successful Reload preserves the Handle identity.
	bool Unload(ActorImprintHandle handle);
	bool Clear();

private:
	// A Slot is a single entry in the ActorImprintSystem's internal storage.
	struct Slot
	{
		std::unique_ptr<const ActorImprint> definition;	// The loaded ActorImprint definition
		Guid assetGuid;									// Guid of the original asset, used for lookup and reload
		std::uint32_t generation = 0;					// Generation of reuse for this handle index
		std::size_t instances = 0;						// Number of live Actor instances materialized from this definition
		bool missing = false;							// Whether the asset is missing
	};

	const AssetManager& m_assets;

	std::vector<Slot> m_slots;								// Internal storage of slots
	std::vector<std::uint32_t> m_freeIndices;				// Indices of free slots
	std::unordered_map<Guid, ActorImprintHandle> m_handles;	// Mapping from asset GUID to handle

	// Flags to track the state of the system during materialization and reloading
	bool m_materializing = false;			// If the system is currently materializing an ActorImprint
	bool m_reloading = false;				// If the system is currently reloading an ActorImprint
	bool m_buildingReloadCandidate = false;	// If the system is currently building a reload candidate

	// Save the handle and definition of the ActorImprint being reloaded
	// to reference it during the reload process.
	ActorImprintHandle m_reloadHandle;
	const ActorImprint* m_reloadDefinition = nullptr;

private:
	friend class ActorImprintInstanceRegistry;
	friend class SceneBase;
	friend class SceneLoader;

	// Defferred destruction of an ActorImprint instance.
	void CommitDestroyInstance(SceneBase& scene, ActorHandle root) noexcept;

	void ReleaseInstance(ActorImprintHandle handle);

	Actor* RestoreInstanceForSceneCandidate(
		SceneBase& scene,
		ActorImprintHandle imprint,
		const ActorImprintRestoreInput& input,
		const std::unordered_set<Guid>& reservedSceneGuids);

	Actor* Materialize(
		SceneBase& scene,
		ActorImprintHandle imprint,
		ActorHandle externalParent,
		const ActorImprintRestoreInput* restoreInput,
		const std::unordered_set<Guid>* reservedSceneGuids);

	const ActorImprint* ResolveForSceneCandidate(ActorImprintHandle handle) const;

};
