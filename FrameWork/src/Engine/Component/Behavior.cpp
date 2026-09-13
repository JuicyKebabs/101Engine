#include "Behavior.h"
#include "Engine/Scene/SceneManager.h"

bool Behavior::ChangeScene(const std::string& sceneName)
{
	SceneManager* sceneManager = GetOwner() && GetOwner()->GetOwner()
		? GetOwner()->GetOwner()->GetSceneManager() : nullptr;
	return sceneManager && sceneManager->ReserveChangeScene(sceneName);
}

bool Behavior::ChangeScene(const Guid& sceneAssetGuid)
{
	SceneManager* sceneManager = GetOwner() && GetOwner()->GetOwner()
		? GetOwner()->GetOwner()->GetSceneManager() : nullptr;
	return sceneManager && sceneManager->ReserveChangeScene(sceneAssetGuid);
}

bool Behavior::ChangeScene(const AssetReference<SceneAsset>& sceneAsset)
{
	return sceneAsset.HasValue() && ChangeScene(sceneAsset.GetGuid());
}
