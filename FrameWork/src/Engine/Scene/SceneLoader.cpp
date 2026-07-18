#include "SceneLoader.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Graphics/LightTypes.h"
#include "nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

static void LoadActor(const nlohmann::json& actorJson, SceneBase* scene, ActorHandle parentHandle);

// Load a scene from a file
bool SceneLoader::LoadScene(const std::string& filePath, SceneBase* scene, EngineContext& context)
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
		LoadActor(actorJson, scene, ActorHandle::Null());
	}

	// Set main camera: find the actor tagged "MainCamera" and register its
	// Camera component with the CameraSystem.
	for (auto& actor : scene->GetAllActors())
	{
		if (actor->GetTag() == ActorTags::MainCamera)
		{
			auto* camera = actor->GetComponentByClass<Camera>();
			if (camera)
			{
				scene->GetCameraSystem()->SetMainCamera(camera);
				DBG("SceneLoader: Main camera set to '%s'", actor->GetName().c_str());
			}
			break;
		}
	}

	if (!scene->GetCameraSystem()->GetMainCamera())
	{
		DBG("SceneLoader: Warning - No main camera found in scene.");
	}

	return true;
}

// Load an actor (Recursive function to load child actors)
void LoadActor(const json& actorJson, SceneBase* scene, ActorHandle parentHandle)
{
	// Prepare an InitDesc for the actor
	Actor::InitDesc desc;
	desc.name = actorJson.value("name", "Actor");
	desc.isActive = actorJson.value("is_active", true);
	std::string tagName = actorJson.value("tag", "None");
	desc.tag = tagName.empty() ? TAG_NONE : TagRegistry::Get().GetId(tagName);

	// Create the actor(with only Transform component)
	auto actorOwned = ActorFactory::CreateEmptyActor(desc);
	Actor* actor = actorOwned.get();

	// Add Transform component
	if (actorJson.contains("transform"))
	{
		auto& t = actorJson["transform"];

		Transform::ParamDesc tdesc;	// Prepare a ParamDesc for the Transform component

		// Set transform parameters from JSON
		tdesc.localPosition =
		{// Local position
			t["position"][0], t["position"][1], t["position"][2],
		};
		tdesc.localEulerDeg =
		{// Local rotation (Euler angles in degrees)
			t["rotation"][0], t["rotation"][1], t["rotation"][2],
		};
		tdesc.localScale =
		{// Local scale
			t["scale"][0], t["scale"][1], t["scale"][2],
		};

		// Get the Transform component and set its parameters
		actor->GetComponentByClass<Transform>()->SetParams(tdesc);
	}

	// Add components to the actor
	if (actorJson.contains("components"))
	{
		for (auto& comp : actorJson["components"])
		{
			std::string name = comp.get<std::string>();	// Get component name from JSON

			// Use the ComponentRegistry to create and add the component to the actor
			if (!ComponentRegistry::Get().AddToActor(name, actor))
			{
				DBG("SceneLoader: Unknown component '%s' for actor '%s'", name.c_str(), desc.name.c_str());
			}
			else
			{
				DBG("SceneLoader: Added component '%s'", name.c_str());
			}
		}
	}

	// Parent-child relationship
	Actor* registered = parentHandle.IsNull() 
		? scene->AddRootActor(std::move(actorOwned)) 
		: scene->AddChildActor(std::move(actorOwned), parentHandle);

	// Recursively load child actors
	if (actorJson.contains("children"))
	{
		for (auto& child : actorJson["children"])
		{
			LoadActor(child, scene, registered->GetHandle());
		}
	}
}
