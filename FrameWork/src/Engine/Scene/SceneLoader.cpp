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
#include <unordered_set>

using json = nlohmann::json;

// Load a scene from a file
bool SceneLoader::LoadScene(const std::string& filePath, SceneBase* scene)
{
	if (!scene)
	{
		DBG("SceneLoader: Scene is null.");
		return false;
	}

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
	try 
	{ 
		j = json::parse(file); 
	}
	catch (const json::exception& e)
	{
		DBG("SceneLoader: Failed to parse JSON: %s", e.what());
		return false;
	}

	// Check scene version
	if (!j.contains("version") || !j["version"].is_number_integer())
	{
		DBG("SceneLoader: Scene file is missing a valid 'version' field.");
		return false;
	}

	const int version = j["version"].get<int>();

	bool loaded = false;

	try
	{// Load the scene based on its version
		switch (version)
		{
		case LEGACY_SCENE_VERSION:
			loaded = LoadSceneVersion2(j, scene);
			break;

		case CURRENT_SCENE_VERSION:
			loaded = LoadSceneVersion3(j, scene);
			break;

		default:
			DBG(
				"SceneLoader: Unsupported scene version: %d",
				version);
			return false;
		}
	}
	catch (const json::exception& exception)
	{// Catch any JSON parsing exceptions and log the error
		DBG(
			"SceneLoader: Invalid scene data: %s",
			exception.what());
		return false;
	}

	if (!loaded) return false;

	// Configure the main camera after loading the scene
	ConfigureMainCamera(scene);

	return true;
}

//----------------------------------------------------------------
// Version 2 scene loading
//----------------------------------------------------------------

bool SceneLoader::LoadSceneVersion2(const json& sceneJson, SceneBase* scene)
{
	std::vector<ActorLoadRecord> records;

	if (!BuildActorLoadRecords(sceneJson, records))
	{
		return false;
	}

	if (!ValidateParentReferences(records))
	{
		return false;
	}

	if (!ValidateHierarchyCycles(records))
	{
		return false;
	}

	// First pass: create every actor as a root.
	for (const auto& record : records)
	{
		if (!RestoreActorDataVersion2(record, scene))
		{
			DBG("SceneLoader: Failed to restore actor.");
			return false;
		}
	}

	// Second pass: resolve Guid references and attach children.
	for (const auto& record : records)
	{
		if (!record.hasParent)
		{
			continue;
		}

		Actor* child = scene->ResolveActor(record.actorGuid);
		Actor* parent = scene->ResolveActor(record.parentGuid);

		if (!child || !parent)
		{
			DBG("SceneLoader: Failed to resolve hierarchy reference.");
			return false;
		}

		child->SetParentHandle(parent->GetHandle());
	}

	if (!ApplySceneSettings(sceneJson, scene))
	{
		DBG("SceneLoader: Failed to apply scene settings.");
		return false;
	}

	return true;
}

Actor* SceneLoader::RestoreActorDataVersion2(
	const ActorLoadRecord& record,
	SceneBase* scene
)
{
	// Construct an InitDesc for the actor using the JSON data
	Actor::InitDesc desc;
	desc.name = record.actorJson->value("name", "Actor");
	desc.isActive = record.actorJson->value("is_active", true);

	const std::string tagName =
		record.actorJson->value("tag", "None");

	desc.tag = tagName.empty()
		? TAG_NONE
		: TagRegistry::Get().GetId(tagName);

	auto actorOwned = ActorFactory::RestoreEmptyActor(
		desc,
		record.actorGuid
	);

	if (!actorOwned)
	{
		DBG("SceneLoader: Failed to restore actor with Guid: %s", record.actorGuid.ToString().c_str());
		return nullptr;
	}

	Actor* actor = actorOwned.get();

	// Add Transform component
	if (record.actorJson->contains("transform"))
	{
		auto& t = (*record.actorJson)["transform"];

		Transform::ParamDesc tdesc;	// Prepare a ParamDesc for the Transform component

		// Set transform parameters from JSON
		tdesc.localPosition =
		{// Local position
			t["position"][0], t["position"][1], t["position"][2],
		};
		const Vector3 localEulerDeg =
		{// Local rotation (Euler angles in degrees)
			t["rotation"][0], t["rotation"][1], t["rotation"][2],
		};
		tdesc.localRotation = Quaternion::CreateFromEulerDeg(localEulerDeg);
		tdesc.localScale =
		{// Local scale
			t["scale"][0], t["scale"][1], t["scale"][2],
		};

		// Get the Transform component and set its parameters
		actor->GetComponentByClass<Transform>()->SetParams(tdesc);
	}

	// Add components to the actor
	if (record.actorJson->contains("components"))
	{
		for (auto& comp : (*record.actorJson)["components"])
		{
			std::string name = comp.get<std::string>();	// Get component name from JSON

			// Use the ComponentRegistry to create and add the component to the actor
			if (!ComponentRegistry::Get().AddToActor(name, actor))
			{
				DBG("SceneLoader: Unknown component '%s' for actor '%s'", name.c_str(), desc.name.c_str());
				return nullptr;
			}

			DBG("SceneLoader: Added component '%s'", name.c_str());
		}
	}

	return scene->AddRootActor(std::move(actorOwned));	// Add the actor to the scene and return the pointer
}

//----------------------------------------------------------------
// Version 3 scene loading
//----------------------------------------------------------------

bool SceneLoader::LoadSceneVersion3(const json& sceneJson, SceneBase* scene)
{
	std::vector<ActorLoadRecord> records;

	if (!BuildActorLoadRecords(sceneJson, records)) return false;

	if (!ValidateParentReferences(records)) return false;

	if (!ValidateHierarchyCycles(records)) return false;

	// First pass: create every actor amd deserialize every component.
	for (const auto& record : records)
	{
		if (!RestoreActorDataVersion3(record, scene))
		{
			DBG("SceneLoader: Failed to restore Version 3 actor.");
			return false;
		}
	}

	// Second pass: resolve actor hierarchy references
	for (const auto& record : records)
	{
		if (!record.hasParent) continue;

		Actor* child = scene->ResolveActor(record.actorGuid);
		Actor* parent = scene->ResolveActor(record.parentGuid);

		if (!child || !parent)
		{
			DBG("SceneLoader: Failed to resolve hierarchy reference.");
			return false;
		}

		child->SetParentHandle(parent->GetHandle());
	}

	// Thirs pass: resolve component references (Actor or assets)
	if (!RestoreComponentReferences(records, scene))
	{
		DBG("SceneLoader: Failed to restore component references.");
		return false;
	}

	// Apply scene settings (e.g., directional light)
	if (!ApplySceneSettings(sceneJson, scene))
	{
		DBG("SceneLoader: Failed to apply Version 3 scene settings.");
		return false;
	}

	return true;
}

Actor* SceneLoader::RestoreActorDataVersion3(const ActorLoadRecord& record, SceneBase* scene)
{
	if (!record.actorJson || !record.actorJson->is_object())
	{
		DBG("SceneLoader: Invalid Version 3 actor record.");
		return nullptr;
	}

	const json& actorJson = *record.actorJson;

	// Build an InitDesc for the actor
	Actor::InitDesc desc;

	desc.name = actorJson.value("name", "Actor");
	desc.isActive = actorJson.value("is_active", true);

	const std::string tagName = actorJson.value("tag", "None");
	desc.tag = tagName.empty() ? TAG_NONE : TagRegistry::Get().GetId(tagName);

	// Create the actor
	auto actorOwned = ActorFactory::RestoreActorShell(desc, record.actorGuid);

	if (!actorOwned)
	{
		DBG("SceneLoader: Failed to restore Version 3 actor : %s", record.actorGuid.ToString().c_str());
		return nullptr;
	}

	Actor* actor = actorOwned.get();

	// Check if the actor has a valid components array
	if (!actorJson.contains("components") || !actorJson["components"].is_array())
	{
		DBG("SceneLoader: Version 3 actor is missing a valid 'components' array.");
		return nullptr;
	}

	bool hasTransformComponent = false;	// Flag to track if a actor has multiple Transform components

	// Deserialize each component in the actor's components array
	for (const json& componentRecord : actorJson["components"])
	{
		if (!componentRecord.is_object())
		{
			DBG("SceneLoader: Version 3 component record must be an object.");
			return nullptr;
		}

		// Validate the component record structure
		if (!componentRecord.contains("type")	 ||
			!componentRecord["type"].is_string() ||
			!componentRecord.contains("data")	 ||
			!componentRecord["data"].is_object())
		{
			DBG("SceneLoader: Invalid Version 3 component record on actor '%s'.", desc.name.c_str());
			return nullptr;
		}

		const std::string componentName = componentRecord["type"].get<std::string>();
		const json& componentData = componentRecord["data"];

		if (componentName.empty())
		{
			DBG("SceneLoader: Empty component type on actor '%s'.", desc.name.c_str());
			return nullptr;
		}

		// Check if the component is a Transform or RectTransform
		const bool isTransformComponent = componentName == "Transform" || componentName == "RectTransform";

		// Check for multiple Transform components on the same actor
		if (isTransformComponent)
		{
			if (hasTransformComponent)
			{
				DBG("SceneLoader: Actor '%s' contains multiple Transform components.", desc.name.c_str());
				return nullptr;
			}

			hasTransformComponent = true;
		}

		// Create the component using the ComponentRegistry
		std::unique_ptr<Component> component(ComponentRegistry::Get().Create(componentName));

		if (!component)
		{
			DBG("SceneLoader: Unknown component '%s' on actor '%s'.", componentName.c_str(), desc.name.c_str());
			return nullptr;
		}

		// Deserialize the component data
		if (!component->Deserialize(componentData))
		{
			DBG("SceneLoader: Failed to deserialize component '%s' on actor '%s'.", componentName.c_str(), desc.name.c_str());
			return nullptr;
		}

		// Add the component to the actor
		if (!actor->AddComponent(std::move(component)))
		{
			DBG("SceneLoader: Failed to add component '%s' to actor '%s'.", componentName.c_str(), desc.name.c_str());
			return nullptr;
		}
	}

	if (!hasTransformComponent)
	{
		DBG( "SceneLoader: Version 3 actor '%s' has neither Transform nor RectTransform.", desc.name.c_str());
		return nullptr;
	}

	// Add the actor to the scene and return the pointer
	return scene->AddRootActor(std::move(actorOwned));
}

bool SceneLoader::RestoreComponentReferences(const std::vector<ActorLoadRecord>& records, SceneBase* scene)
{
	if (!scene) return false;

	for (auto& record : records)
	{
		// Resolve the actor for the current record
		Actor* actor = scene->ResolveActor(record.actorGuid);

		if (!actor)
		{
			DBG("SceneLoader: Failed to resolve actor for component reference restoration.");
			return false;
		}

		// Resolve references for each component of the actor
		for (Component* component : actor->GetAllComponents())
		{
			if (!component || component->IsDestroyed()) continue;

			if (!component->ResolveReferences(*scene))
			{
				const std::type_index typeId(typeid(*component));
				const std::string typeName = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

				DBG(
					"SceneLoader: Failed to resolve component '%s' references on actor '%s'.",
					typeName.empty()
					? typeId.name()
					: typeName.c_str(),
					actor->GetName().c_str());

				return false;
			}
		}
	}

	return true;
}


bool SceneLoader::ApplySceneSettings(const json& sceneJson, SceneBase* scene)
{
	// DirectionalLight
	if (sceneJson.contains("directional_light"))
	{
		auto& light = sceneJson["directional_light"];
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
	else
	{
		DBG("SceneLoader: Warning - No directional light found in scene settings.");
	}

	return true;
}

void SceneLoader::ConfigureMainCamera(SceneBase* scene)
{
	for (Actor* actor : scene->GetAllActors())
	{
		if (!actor || actor->IsDestroyed()) continue;
		if (actor->GetTag() != ActorTags::MainCamera) continue;

		Camera* camera = actor->GetComponentByClass<Camera>();

		if (!camera) continue;

		scene->GetCameraSystem()->SetMainCamera(camera);

		DBG(
			"SceneLoader: Main camera set to '%s'",
			actor->GetName().c_str());

		return;
	}

	DBG("SceneLoader: Warning - No main camera found.");
}


bool SceneLoader::BuildActorLoadRecords(
	const json& sceneJson,
	std::vector<ActorLoadRecord>& outRecords
)
{
	// Check if the scene JSON contains an "actors" array
	if (!sceneJson.contains("actors") || !sceneJson["actors"].is_array())
	{
		DBG("SceneLoader: 'actors' must be an array.");
		return false;
	}

	std::unordered_set<Guid> actorGuids;	// To check for duplicate GUIDs

	for (const auto& actorJson : sceneJson["actors"])
	{
		// Check if the actor entry is a valid JSON object
		if (!actorJson.is_object())
		{
			DBG("SceneLoader: Actor entry must be an object.");
			return false;
		}

		// Check if the actor has a valid "actorId" field
		if (!actorJson.contains("actorId") || !actorJson["actorId"].is_string())
		{
			DBG("SceneLoader: Actor is missing a valid actorId.");
			return false;
		}

		// Parse the actor's GUID from the "actorId" field
		Guid actorGuid;
		if (!Guid::TryParse(
			actorJson["actorId"].get<std::string>(),
			actorGuid))
		{
			DBG("SceneLoader: Actor contains an invalid actorId.");
			return false;
		}

		// Check for duplicate actor GUIDs
		if (!actorGuids.emplace(actorGuid).second)
		{
			DBG(
				"SceneLoader: Duplicate actorId: %s",
				actorGuid.ToString().c_str());
			return false;
		}

		// Create an ActorLoadRecord after cheching GUID is valid and unique

		ActorLoadRecord record;
		record.actorJson = &actorJson;
		record.actorGuid = actorGuid;

		// Check if the actor has a parent GUID field
		// Not if the actor has a parent, but if the actor has a parentId field in the JSON data
		if (!actorJson.contains("parentId"))
		{
			DBG("SceneLoader: Actor is missing parentId.");
			return false;
		}

		// Check if the actor has parent
		if (actorJson["parentId"].is_null())
		{// No parent
			record.hasParent = false;
		}
		else if (actorJson["parentId"].is_string())
		{// Has parent
			Guid parentGuid;

			// Parse the parent GUID from the "parentId" field
			if (!Guid::TryParse(
				actorJson["parentId"].get<std::string>(),
				parentGuid))
			{
				DBG("SceneLoader: Actor contains an invalid parentId.");
				return false;
			}

			// Check if the actor is its own parent
			if (parentGuid == actorGuid)
			{
				DBG("SceneLoader: Actor cannot be its own parent.");
				return false;
			}

			// Set the parent information in the record
			record.hasParent = true;
			record.parentGuid = parentGuid;
		}
		else
		{// in case of invalid parentId type (not string or null)
			DBG("SceneLoader: parentId must be a Guid string or null.");
			return false;
		}

		outRecords.push_back(record);	// Add the record to the output vector
	}

	return true;
}

bool SceneLoader::HasHierarchyCycle(
	const Guid& actorGuid,
	const std::unordered_map<Guid, Guid>& parentMap,
	std::unordered_map<Guid, VisitState>& states)
{
	VisitState& state = states[actorGuid];

	// This parent actor is currently being visited, which indicates a cycle in the hierarchy
	if (state == VisitState::Visiting) return true;

	// This actor has already been fully visited and no cycle was detected in its path, so no cycle is detected in this path
	if (state == VisitState::Visited) return false;

	// This actor has not been visited yet, so mark it as currently being visited
	state = VisitState::Visiting;

	// Serch for the parent of this actor in the parent map
	auto parentIt = parentMap.find(actorGuid);
	if (parentIt != parentMap.end())
	{// If exists, recursively check the parent actor for cycles
		if (HasHierarchyCycle(parentIt->second, parentMap, states))
		{
			return true;
		}
	}

	// If no cycle was detected in the parent path, mark this actor as fully visited
	state = VisitState::Visited;

	return false;
}

bool SceneLoader::ValidateParentReferences(const std::vector<ActorLoadRecord>& records)
{
	std::unordered_set<Guid> actorGuids;	// To check for missing parents
	for (const auto& record : records)
	{
		actorGuids.insert(record.actorGuid);	// Collect all actor GUIDs
	}

	for (const auto& record : records)
	{
		if (record.hasParent && actorGuids.find(record.parentGuid) == actorGuids.end())
		{
			DBG(
				"SceneLoader: Parent Guid does not exist: %s",
				record.parentGuid.ToString().c_str());
			return false;
		}
	}

	return true;	// All parent references are valid
}

bool SceneLoader::ValidateHierarchyCycles(const std::vector<ActorLoadRecord>& records)
{
	std::unordered_map<Guid, Guid> parentMap;

	// Pick up all parent-child relationships from the records and store them in a map for cycle detection
	for (const auto& record : records)
	{
		if (record.hasParent) parentMap.emplace(record.actorGuid, record.parentGuid);
	}

	std::unordered_map<Guid, VisitState> states;	// Place to save the visit state of each actor during the cycle detection process

	// Check each actor for cycles in the hierarchy
	for (const auto& record : records)
	{
		if (HasHierarchyCycle(record.actorGuid, parentMap, states))
		{
			DBG("SceneLoader: Actor hierarchy contains a cycle.");
			return false;
		}
	}

	return true;	// No cycles detected in the hierarchy
}
