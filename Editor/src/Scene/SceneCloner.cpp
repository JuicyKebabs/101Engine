#include "SceneCloner.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Scene/SceneWriter.h"
#include "Engine/Scene/SceneLoader.h"
#include "nlohmann/json.hpp"
#include <utility>

using json = nlohmann::json;

std::unique_ptr<SceneBase> SceneCloner::Clone(const SceneBase* sourceScene, EngineContext& context)
{
	if (!sourceScene)
	{
		return nullptr;
	}

	json j;

	if (!SceneWriter::SerializeScene(sourceScene, j))
	{
		return nullptr;
	}

	SceneLoadResult result = SceneLoader::LoadCandidate(j, context, "<scene-clone>");
	return std::move(result.scene);
}
