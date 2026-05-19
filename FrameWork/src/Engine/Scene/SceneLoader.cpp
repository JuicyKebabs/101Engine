#include "SceneLoader.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/EngineComponentRegistry.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Graphics/LightTypes.h"
#include "nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

bool SceneLoader::Load(const std::string& filePath, SceneBase* scene, EngineContext& context)
{
	// Open the scene file
	std::string fullPath = PathManager::Resolve(filePath);
	std::ifstream file(fullPath);
	if (!file.is_open())
	{
		DBG("SceneLoader: Failed to open scene file: %s", fullPath.c_str());
		return false;
	}

	// Parse JSON data from the file
	json j;
	try { j = json::parse(file); }
	catch (const json::exception& e)
	{
		DBG("SceneLoader: Failed to parse JSON: %s", e.what());
		return false;
	}

	// Check scene version
	if (j["version"] != CURRENT_SCENE_VERSION)
	{
		DBG("SceneLoader: Unsupported scene version: Expected %d file version: %d", CURRENT_SCENE_VERSION, j["version"].get<int>());
		return false;
	}

	// DirectionalLight
	if (j.contains("directional_light"))
	{
		auto& light = j["directional_light"];
		DirectionalLight dl;
		dl.direction = {
			light["direction"][0], light["direction"][1], light["direction"][2]
		};
		dl.color = {
			light["color"][0], light["color"][1], light["color"][2]
		};
		dl.intensity = light["intensity"];
		scene->SetDirectionalLight(dl);
	}
}