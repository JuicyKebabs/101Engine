#include "SceneCloner.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Scene/SceneWriter.h"
#include "Engine/Scene/SceneLoader.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

std::unique_ptr<SceneBase> SceneCloner::Clone(const SceneBase* sourceScene, EngineContext& context)
{
	if (!sourceScene) return nullptr;

	json j;

	if (!SceneWriter::SerializeScene(sourceScene, j)) return nullptr;

	auto clonedScene = std::make_unique<SceneBase>();
	clonedScene->Initialize(context);

	if (!SceneLoader::DeserializeScene(clonedScene.get(), j))
	{
		clonedScene->Finalize();
		return nullptr;
	}

	return clonedScene;
}
