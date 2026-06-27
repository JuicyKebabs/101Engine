#include "Behavior.h"
#include "Engine/Scene/SceneManager.h"

void Behavior::ChangeScene(const std::string& sceneName)
{
	// Get the SceneManager instance from the EngineContext
	SceneManager* sceneManager = GetOwner()->GetOwner()->GetSceneManager();
	if (sceneManager)
	{
		// Reserve a scene change to the specified scene name
		sceneManager->ReserveChangeScene(sceneName);
	}
}