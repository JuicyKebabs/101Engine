#include "SceneWriter.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/Camera.h"
#include "Engine/Graphics/LightTypes.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include "nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

static json SerializeActor(Actor* actor);

// Save a scene to a file
bool SceneWriter::SaveScene(const std::string& filePath, SceneBase* scene)
{
	// Check if the scene has a main camera before attempting to save
	bool hasMainCamera = false;
	for (auto& actor : scene->GetAllActors())
	{
		if (actor->GetTag() == ActorTags::MainCamera)
		{
			if (actor->HasComponent<Camera>())
			{
				hasMainCamera = true;
				break;
			}
		}
	}

	if (!hasMainCamera)
	{// If no main camera is found, log a warning and return false to indicate failure
		DBG("SceneWriter: Save failed - No main camera in scene.");
		return false;
	}

	json j;

	// Version
	j["version"] = CURRENT_VERSION;

	// Directional light
	const auto& dl = scene->GetDirectionalLight();
	j["directional_light"] = {
		{"direction", { dl.direction.x, dl.direction.y, dl.direction.z }},
		{"color",     { dl.color.x, dl.color.y, dl.color.z }},
		{"intensity", dl.intensity}
	};

	// Actors (only root actors are serialized here; child actors will be serialized recursively as part of their parents)
	j["actors"] = json::array();
	for(auto& actor : scene->GetRootActors())
	{
		j["actors"].push_back(SerializeActor(actor));
	}

	// Resolve the full path for the output file and open it for writing
	std::string fullPath = PathManager::Resolve(filePath);
	std::ofstream file(fullPath);

	// Check if the file was opened successfully
	if (!file.is_open())
	{
		DBG("SceneWriter: Failed to open file for writing: %s", fullPath.c_str());
		return false;
	}

	// Write the JSON data to the file with pretty printing
	file << j.dump(4); // Pretty print with 4 spaces indent
	DBG("SceneWriter: Scene saved successfully to %s", fullPath.c_str());
	return true;
}

static json SerializeActor(Actor* actor)
{
	json j;

	// Basic properties
	j["name"] = actor->GetName();								// Actor name
	j["is_active"] = actor->IsActive();							// Active status
	j["tag"] = TagRegistry::Get().GetName(actor->GetTag());		// Tag name

	// Transform component
	auto* t = actor->GetComponentByClass<Transform>();
	if (t)
	{
		// Get local transform properties
		Vector3 pos = t->GetLocalPosition();
		Vector3 rot = t->GetLocalRotationEulerDeg();
		Vector3 scale = t->GetLocalScale();

		// Serialize transform as a nested object
		j["transform"] = {
			{"position", { pos.x, pos.y, pos.z }},
			{"rotation", { rot.x, rot.y, rot.z }},
			{"scale",    { scale.x, scale.y, scale.z }}
		};
	}

	// Components
	j["components"] = json::array();
	auto typeIds = actor->GetComponentsTypeIds();
	for(auto& typeId : typeIds)
	{
		// Skip transform component
		if (typeId == std::type_index(typeid(Transform))) continue;

		// Get the registered name for the component type index
		std::string name = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

		// Check if a registered name was found and add it to the components array
		if (!name.empty())
		{
			j["components"].push_back(name);
		}
		 else
		{
			DBG("SceneWriter: No registered name found for component type index '%s'", typeId.name());
		}
	}

	// Child actors (recursively serialize children)
	j["children"] = json::array();
	for(auto& child : actor->GetDirectChildren())
	{
		j["children"].push_back(SerializeActor(child));
	}

	return j;
}
