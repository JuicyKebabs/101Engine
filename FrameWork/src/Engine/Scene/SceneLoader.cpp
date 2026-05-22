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

	// Load actors
	for (auto& actorJson : j["actors"])
	{
		LoadActor(actorJson, scene, nullptr);
	}

	return true;
}

// Load an actor (Recursive function to load child actors)
static void LoadActor(const json& actorJson, SceneBase* scene, Actor* parent)
{
	// Prepare an InitDesc for the actor
	Actor::InitDesc desc;
	desc.name = actorJson.value("name", "Actor");
	desc.isActive = actorJson.value("is_active", true);
	std::string tagName = actorJson.value("tag", "None");
	desc.tag = tagName.empty() ? TAG_NONE : TagRegistry::Get().GetId(tagName);

	// Create the actor using the factory
	Actor* actor = ActorFactory::CreateEmptyActor(desc);

	// Add Transform component
	if (actorJson.contains("transform"))
	{
		auto& t = actorJson["transform"];
		Transform::ParamDesc tdesc;
		tdesc.localPosition = {
			t["position"][0], t["position"][1], t["position"][2],
		};
		tdesc.localEulerDeg = {
			t["rotation"][0], t["rotation"][1], t["rotation"][2],
		};
		tdesc.localScale = {
			t["scale"][0], t["scale"][1], t["scale"][2],
		};
		actor->GetComponentByClass<Transform>()->SetParams(tdesc);
	}

	// Add Built-in ngine components
	if (actorJson.contains("components"))
	{
		for (auto& comp : actorJson["components"])
		{
			std::string name = comp.get<std::string>();
			if (!EngineComponentRegistry::Get().Add(name, actor))
			{
				DBG("SceneLoader: Unknown engine component '%s' for actor '%s'", name.c_str(), desc.name.c_str());
			}
		}
	}

	// Add user-defined behavior components
	if (actorJson.contains("behaviors"))
	{
		for (auto& behavior : actorJson["behaviors"])
		{
			std::string name = behavior.get<std::string>();
			Behavior* instance = ComponentRegistry::Get().Create(name);
			if(instance)
			{
				actor->AddComponentInstance(std::unique_ptr<Component>(instance));
			}
			else
			{
				DBG("SceneLoader: Unknown behavior component '%s' for actor '%s'", name.c_str(), desc.name.c_str());
			}
		}
	}

	// Parent-child relationship
	if(parent)
	{
		parent->SetParent(parent);
	}

	// Add this actor to the scene
	scene->AddActor(actor);

	// Recursively load child actors
	if (actorJson.contains("children"))
	{
		for (auto& child : actorJson["children"])
		{
			LoadActor(child, scene, actor);
		}
	}
}