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

class ActorImprintEditingContext
{
public:
	static std::unique_ptr<ActorImprintEditingContext> Open(
		const Guid& assetGuid,
		AssetManager& assets,
		ActorImprintSystem& system,
		EngineContext& engineContext);
	~ActorImprintEditingContext();

	ActorImprintEditingContext(const ActorImprintEditingContext&) = delete;
	ActorImprintEditingContext& operator=(const ActorImprintEditingContext&) = delete;

	SceneBase* GetWorkingScene() const { return m_workingScene.get(); }
	std::unique_ptr<SceneBase>* GetWorkingSceneOwnerSlot() { return &m_workingScene; }
	ActorImprintEditingObjectMap& GetObjectMap() { return m_objectMap; }
	const ActorImprintEditingObjectMap& GetObjectMap() const { return m_objectMap; }
	const Guid& GetAssetGuid() const { return m_assetGuid; }
	const std::string& GetAssetPath() const { return m_assetPath; }

	std::unique_ptr<const ActorImprint> CaptureSnapshot(nlohmann::json& outJson) const;
	bool Save();
	void CommitPendingSave();
	bool RollbackPendingSave();
	bool HasPendingSave() const { return m_hasPendingSave; }

private:
	ActorImprintEditingContext(
		Guid assetGuid,
		std::string assetPath,
		AssetManager& assets,
		std::unique_ptr<SceneBase> workingScene,
		ActorImprintEditingObjectMap objectMap,
		nlohmann::json savedSnapshot);

	Guid m_assetGuid;
	std::string m_assetPath;
	AssetManager* m_assets;
	std::unique_ptr<SceneBase> m_workingScene;
	ActorImprintEditingObjectMap m_objectMap;
	nlohmann::json m_savedSnapshot;
	nlohmann::json m_preSaveSnapshot;
	std::string m_preSaveBytes;
	bool m_hasPendingSave = false;
};
