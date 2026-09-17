#include "Behavior.h"
#include "Engine/Scene/SceneManager.h"

bool Behavior::ChangeScene(const std::string& sceneName)
{
	if (!GetOwner() || !GetOwner()->GetOwner())
	{
		return false;
	}

	SceneManager* sceneManager = GetOwner()->GetOwner()->GetSceneManager();
	return sceneManager && sceneManager->ReserveChangeScene(sceneName);
}

bool Behavior::ChangeScene(const Guid& sceneAssetGuid)
{
	if (!GetOwner() || !GetOwner()->GetOwner())
	{
		return false;
	}

	SceneManager* sceneManager = GetOwner()->GetOwner()->GetSceneManager();
	return sceneManager && sceneManager->ReserveChangeScene(sceneAssetGuid);
}

bool Behavior::ChangeScene(const AssetReference<SceneAsset>& sceneAsset)
{
	return sceneAsset.HasValue() && ChangeScene(sceneAsset.GetGuid());
}
