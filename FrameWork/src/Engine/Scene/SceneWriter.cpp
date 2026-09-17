#include "SceneWriter.h"
#include "SceneVersion.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/ActorSerializer.h"
#include "Engine/ActorImprint/ActorImprintInstanceSerializer.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Camera.h"
#include "Engine/Graphics/LightTypes.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

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

bool SceneWriter::SerializeScene(const SceneBase* scene, json& outJson)

{
	return SerializeSceneImpl(scene, outJson, false);
}

bool SceneWriter::SerializeReloadSnapshot(const SceneBase* scene, json& outJson)
{
	return SerializeSceneImpl(scene, outJson, true);
}

bool SceneWriter::SerializeSceneImpl(const SceneBase* scene, json& outJson, bool forReload)
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

	if (!forReload && !hasMainCamera)
	{// If no main camera is found, log a warning and return false to indicate failure
		DBG("SceneWriter: Scene serialization failed - No main camera in scene.");
		return false;
	}

	json j;

	// Version
	j["version"] = CURRENT_SCENE_VERSION;

	// Directional light
	const auto& dl = scene->GetDirectionalLight();

	if (!std::isfinite(dl.direction.x) || !std::isfinite(dl.direction.y) ||
		!std::isfinite(dl.direction.z) || !std::isfinite(dl.color.x) ||
		!std::isfinite(dl.color.y) || !std::isfinite(dl.color.z) ||
		!std::isfinite(dl.intensity))
	{
		DBG("SceneWriter: Scene serialization failed - Directional light contains a non-finite value.");
		return false;
	}

	j["directional_light"] = {
		{"direction", { dl.direction.x, dl.direction.y, dl.direction.z }},
		{"color",     { dl.color.x, dl.color.y, dl.color.z }},
		{"intensity", dl.intensity}
	};

	j["actors"] = json::array();
	std::vector<Actor*> ordinaryActors;

	for (Actor* actor : scene->GetAllActors())
	{
		if (!actor || actor->IsDestroyed())
		{
			continue;
		}

		if (scene->GetImprintInstances().FindMember(actor->GetHandle()))
		{
			continue;
		}

		ordinaryActors.push_back(actor);
	}

	std::sort(ordinaryActors.begin(), ordinaryActors.end(), [](const Actor* left, const Actor* right)
	{
		return left->GetGuid().ToString() < right->GetGuid().ToString();
	});

	for (Actor* actor : ordinaryActors)
	{
		// Serialize the actor and add it to the JSON array
		json actorJson;

		if (!ActorSerializer::SerializeActorRecord(actor, scene, actorJson))
		{
			DBG("SceneWriter: Failed to serialize an actor.");
			return false;
		}

		j["actors"].push_back(std::move(actorJson));
	}

	j["actorImprintInstances"] = json::array();
	std::vector<ActorHandle> instanceRoots;
	instanceRoots.reserve(scene->GetImprintInstances().GetInstances().size());

	for (const auto& [root, record] : scene->GetImprintInstances().GetInstances())
	{
		if (!forReload)
		{
			const EngineContext* context = scene->GetEngineContext();

			if (!context || !context->pActorImprintSystem ||
				context->pActorImprintSystem->IsMissing(record.imprint))
			{
				DBG("SceneWriter: Scene serialization failed - ActorImprint asset is missing.");
				return false;
			}
		}

		instanceRoots.push_back(root);
	}

	std::sort(instanceRoots.begin(), instanceRoots.end(), [scene](ActorHandle left, ActorHandle right)
	{
		const Actor* leftActor = scene->ResolveActor(left);
		const Actor* rightActor = scene->ResolveActor(right);

		if (!leftActor || !rightActor)
		{
			if (leftActor != rightActor)
			{
				return leftActor != nullptr;
			}

			if (left.index != right.index)
			{
				return left.index < right.index;
			}

			return left.generation < right.generation;
		}

		return leftActor->GetGuid().ToString() < rightActor->GetGuid().ToString();
	});

	for (ActorHandle root : instanceRoots)
	{
		json instanceJson;

		if (!ActorImprintInstanceSerializer::Serialize(*scene, root, instanceJson))
		{
			return false;
		}

		j["actorImprintInstances"].push_back(std::move(instanceJson));
	}

	outJson = std::move(j);
	return true;
}
