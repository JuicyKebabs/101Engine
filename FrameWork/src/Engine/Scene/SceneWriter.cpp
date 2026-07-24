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

static bool SerializeActor(
	Actor* actor,
	SceneBase* scene,
	json& outJson
);

// Save a scene to a file
bool SceneWriter::SaveScene(const std::string& filePath, SceneBase* scene)
{
	// Check if the scene pointer is valid before proceeding
	if (!scene)
	{
		DBG("SceneWriter: Save failed - Scene is null.");
		return false;
	}

	// Check if the scene has a main camera before attempting to save
	bool hasMainCamera = false;
	for (auto& actor : scene->GetAllActors())
	{
		// Check if the actor is valid and not destroyed before checking for the main camera
		if (!actor || actor->IsDestroyed())
		{
			continue;
		}

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
	j["version"] = CURRENT_SCENE_VERSION;

	// Directional light
	const auto& dl = scene->GetDirectionalLight();
	j["directional_light"] = {
		{"direction", { dl.direction.x, dl.direction.y, dl.direction.z }},
		{"color",     { dl.color.x, dl.color.y, dl.color.z }},
		{"intensity", dl.intensity}
	};

	j["actors"] = json::array();
	for(auto& actor : scene->GetAllActors())
	{
		// Check if the actor is valid and not destroyed before serializing
		if (!actor || actor->IsDestroyed())
		{
			continue;
		}

		// Serialize the actor and add it to the JSON array
		json actorJson;
		if (!SerializeActor(actor, scene, actorJson))
		{
			DBG("SceneWriter: Failed to serialize an actor.");
			return false;
		}

		j["actors"].push_back(std::move(actorJson));
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

static bool SerializeActor(Actor* actor, SceneBase* scene, json& outJson)
{
	if (!actor)
	{
		DBG("SceneWriter::SerializeActor: Actor is null.");
		return false;
	}

	json j;

	// Get the actor's GUID and check if it's valid
	const Guid& actorGuid = actor->GetGuid();
	if (!actorGuid.IsValid())
	{
		DBG(
			"SceneWriter::SerializeActor: Actor '%s' has an invalid Guid.",
			actor->GetName().c_str());
		return false;
	}

	// Save the actor's GUID as a string in the JSON object
	j["actorId"] = actorGuid.ToString();


	// Get parent actor's GUID if it exists
	const ActorHandle parentHandle = actor->GetParentHandle();
	if (parentHandle.IsNull())
	{
		j["parentId"] = nullptr; // No parent
	}
	else
	{
		Actor* parent = scene->ResolveActor(parentHandle);

		// Check if parent actor is exists
		if (!parent)
		{
			DBG(
				"SceneWriter::SerializeActor: Parent of Actor '%s' cannot be resolved.",
				actor->GetName().c_str());
			return false;
		}

		// Check if parent actor's GUID is valid
		if (!parent->GetGuid().IsValid())
		{
			DBG(
				"SceneWriter::SerializeActor: Parent of Actor '%s' has an invalid Guid.",
				actor->GetName().c_str());
			return false;
		}

		// Save the parent actor's GUID as a string in the JSON object
		j["parentId"] = parent->GetGuid().ToString();
	}

	// Basic properties
	j["name"] = actor->GetName();								// Actor name
	j["is_active"] = actor->IsActive();							// Active status
	j["tag"] = TagRegistry::Get().GetName(actor->GetTag());		// Tag name

	// Components
	j["components"] = json::array();

	for (Component* component : actor->GetAllComponents())
	{
		if (!component || component->IsDestroyed()) continue;

		// Get the type index and name of the component from the registry
		const std::type_index typeId(typeid(*component));
		const std::string typeName = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

		if (typeName.empty())
		{
			DBG("SceneWriter: No registered name found for component type '%s'.", typeId.name());
			return false;
		}

		// Serialize the component's data into a JSON object
		json componentData;
		if (!component->Serialize(componentData))
		{
			DBG("SceneWriter: Failed to serialize component '%s' on actor '%s'.", typeName.c_str(), actor->GetName().c_str());
			return false;
		}

		// Create a JSON object for the component with its type and serialized data
		json componentRecord;
		componentRecord["type"] = typeName;
		componentRecord["data"] = std::move(componentData);

		j["components"].push_back(std::move(componentRecord));
	}

	outJson = std::move(j);
	return true;
}
