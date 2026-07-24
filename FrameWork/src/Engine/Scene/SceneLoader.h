#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "SceneVersion.h"
#include "Engine/Core/GUID/Guid.h"
#include "nlohmann/json.hpp"

//-----------------------------------------------------------------------------
// SceneLoader class
// This class is responsible for loading scene data from a file.
//------------------------------------------------------------------------------

class SceneBase;
class Actor;
class EngineContext;

class SceneLoader
{
public:
	static bool LoadScene(const std::string& filePath, SceneBase* scene);

private:
	// Structure to hold information about an actor being loaded
	struct ActorLoadRecord
	{
		const nlohmann::json* actorJson = nullptr;	// Pointer to the actor's JSON data
		Guid actorGuid;						// Actor's GUID
		bool hasParent = false;				// Whether the actor has a parent
		Guid parentGuid;					// Parent actor's GUID (if any)
	};

	// Enum to represent the visit state of an actor reference during the loading process
	// Used to avoid circular dependencies
	enum class VisitState
	{
		Unvisited,	// Actor has not been visited yet
		Visiting,	// Actor is currently being visited (in the process of loading)
		Visited		// Actor has been fully visited (loading complete)
	};

private:
	// Scene loader for  version 2
	static bool LoadSceneVersion2(
		const nlohmann::json& sceneJson,
		SceneBase* scene
	);

	// Restore actor data for version 2
	static Actor* RestoreActorDataVersion2(
		const ActorLoadRecord& record,
		SceneBase* scene
	);

	// Scene loader for version 3
	static bool LoadSceneVersion3(
		const nlohmann::json& sceneJson,
		SceneBase* scene
	);

	// Restore actor data for version 3
	static Actor* RestoreActorDataVersion3(
		const ActorLoadRecord& record,
		SceneBase* scene
	);

	// Restore component references for all actors after they have been created
	static bool RestoreComponentReferences(
		const std::vector<ActorLoadRecord>& records,
		SceneBase* scene
	);

	// Apply scene settings from the JSON data to the scene
	static bool ApplySceneSettings(
		const nlohmann::json& sceneJson,
		SceneBase* scene
	);

	// Check if the scene has a main camera and configure it if necessary
	static void ConfigureMainCamera(SceneBase* scene);

	// Build a list of actor load records from the scene JSON data before creating the actors
	static bool BuildActorLoadRecords(
		const nlohmann::json& sceneJson,
		std::vector<ActorLoadRecord>& outRecords
	);

	// Check if the actor hierarchy has cycles (circular dependencies)
	static bool HasHierarchyCycle(
		const Guid& actorGuid,
		const std::unordered_map<Guid, Guid>& parentMap,
		std::unordered_map<Guid, VisitState>& states
	);

	// Check if the parent references in the actor load records are valid (no missing parents)
	static bool ValidateParentReferences(const std::vector<ActorLoadRecord>& records);
	
	// Check if the actor hierarchy is valid and does not contain cycles (circular dependencies)
	static bool ValidateHierarchyCycles(const std::vector<ActorLoadRecord>& records);
};
