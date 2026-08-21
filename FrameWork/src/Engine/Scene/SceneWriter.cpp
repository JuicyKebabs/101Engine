#include "SceneWriter.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/ActorSerializer.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Camera.h"
#include "Engine/Graphics/LightTypes.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <fstream>

using json = nlohmann::json;

// Save a scene to a file
bool SceneWriter::SaveScene(const std::string& filePath, SceneBase* scene)
{
	json j;

	if (!SerializeScene(scene, j))
	{
		DBG("SceneWriter: Failed to create scene JSON.");
		return false;
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

bool SceneWriter::SerializeScene(SceneBase* scene, json& outJson)
{
	// Check if the scene pointer is valid before proceeding
	if (!scene)
	{
		DBG("SceneWriter: Scene serialization failed - Scene is null.");
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
		DBG("SceneWriter: Scene serialization failed - No main camera in scene.");
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
	for (auto& actor : scene->GetAllActors())
	{
		// Check if the actor is valid and not destroyed before serializing
		if (!actor || actor->IsDestroyed())
		{
			continue;
		}

		// Serialize the actor and add it to the JSON array
		json actorJson;
		if (!ActorSerializer::SerializeActorRecord(actor, scene, actorJson))
		{
			DBG("SceneWriter: Failed to serialize an actor.");
			return false;
		}

		j["actors"].push_back(std::move(actorJson));
	}

	outJson = std::move(j);
	return true;
}