#pragma once

#include "ActorImprintEditingObjectMap.h"
#include "ActorImprintEditingSnapshot.h"
#include "Engine/Core/GUID/Guid.h"

#include <memory>
#include <string>

class ActorImprintSystem;
class AssetManager;
struct EngineContext;
class SceneBase;

enum class ActorImprintEditingSaveErrorCode
{
	None,
	InvalidContext,
	SnapshotFailed,
	TemporaryFileFailed,
	TemporaryValidationFailed,
	AtomicReplaceFailed,
	NotificationFailed,
	RollbackFailed,
};

struct ActorImprintEditingSaveError
{
	ActorImprintEditingSaveErrorCode code = ActorImprintEditingSaveErrorCode::None;
	std::string path;
	std::string message;
};

struct ActorImprintEditingOpenError
{
	std::string path;
	std::string message;
};

class ActorImprintEditingContext
{
public:
	static std::unique_ptr<ActorImprintEditingContext> Open(
		const Guid& assetGuid, AssetManager& assets, ActorImprintSystem& system,
		EngineContext& engineContext, ActorImprintEditingOpenError* outError = nullptr);
	~ActorImprintEditingContext();

	ActorImprintEditingContext(const ActorImprintEditingContext&) = delete;
	ActorImprintEditingContext& operator=(const ActorImprintEditingContext&) = delete;

	SceneBase* GetWorkingScene() const { return m_workingScene.get(); }
	std::unique_ptr<SceneBase>* GetWorkingSceneOwnerSlot() { return &m_workingScene; }
	ActorImprintEditingObjectMap& GetObjectMap() { return m_objectMap; }
	const ActorImprintEditingObjectMap& GetObjectMap() const { return m_objectMap; }
	const Guid& GetAssetGuid() const { return m_assetGuid; }
	const std::string& GetAssetPath() const { return m_assetPath; }

	std::unique_ptr<const ActorImprint> CaptureSnapshot(nlohmann::json& outJson,
		ActorImprintEditingSnapshotError* outError = nullptr) const;
	bool Save(ActorImprintEditingSaveError* outError = nullptr);
	void CommitPendingSave();
	bool RollbackPendingSave(ActorImprintEditingSaveError* outError = nullptr);
	bool HasPendingSave() const { return m_hasPendingSave; }
	const ActorImprintEditingSaveError& GetLastSaveError() const { return m_lastSaveError; }

private:
	ActorImprintEditingContext(Guid assetGuid, std::string assetPath,
		AssetManager& assets, std::unique_ptr<SceneBase> workingScene,
		ActorImprintEditingObjectMap objectMap, nlohmann::json savedSnapshot);

	Guid m_assetGuid;
	std::string m_assetPath;
	AssetManager* m_assets;
	std::unique_ptr<SceneBase> m_workingScene;
	ActorImprintEditingObjectMap m_objectMap;
	nlohmann::json m_savedSnapshot;
	nlohmann::json m_preSaveSnapshot;
	std::string m_preSaveBytes;
	bool m_hasPendingSave = false;
	ActorImprintEditingSaveError m_lastSaveError;
};
