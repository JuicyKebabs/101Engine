#include "ActorSerializer.h"
#include "ComponentSerializer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Core/Debug/Debug.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

bool ActorSerializer::SerializeActorRecord(Actor* actor, const SceneBase* scene, json& outJson)
{
	if (!actor)
	{
		DBG("ActorSerializer::SerializeActorRecord: Actor is null.");
		return false;
	}

	if (!scene)
	{
		DBG("ActorSerializer::SerializeActorRecord: Scene is null.");
		return false;
	}

	json j;

	// Serialize the actor's GUID and check if it's valid
	const Guid& actorGuid = actor->GetGuid();
	if (!actorGuid.IsValid())
	{
		DBG("ActorSerializer::SerializeActorRecord: Actor '%s' has an invalid Guid.", actor->GetName().c_str());
		return false;
	}

	j["actorId"] = actorGuid.ToString();

	// Serialize the parent actor's GUID if it exists
	const ActorHandle parentHandle = actor->GetParentHandle();
	if (parentHandle.IsNull())
	{
		j["parentId"] = nullptr; // No parent
	}
	else
	{
		Actor* parent = scene->ResolveActor(parentHandle);
		if (!parent)
		{
			DBG("ActorSerializer::SerializeActorRecord: Parent of Actor '%s' cannot be resolved.", actor->GetName().c_str());
			return false;
		}
		if (!parent->GetGuid().IsValid())
		{
			DBG("ActorSerializer::SerializeActorRecord: Parent of Actor '%s' has an invalid Guid.", actor->GetName().c_str());
			return false;
		}
		j["parentId"] = parent->GetGuid().ToString();
	}

	// Serialize other actor properties
	j["name"] = actor->GetName();
	j["is_active"] = actor->IsActive();
	j["tag"] = TagRegistry::Get().GetName(actor->GetTag());

	// Serialize components
	j["components"] = json::array();

	for (Component* component : actor->GetAllComponents())
	{
		if (!component || component->IsDestroyed()) continue;

		json componentJson;

		// Serialize the component using ComponentSerializer
		if (!ComponentSerializer::SerializeRecord(component, componentJson))
		{
			DBG(
				"ActorSerializer::SerializeActorRecord: Failed to serialize a component on Actor '%s'.",
				actor->GetName().c_str()
			);
			return false;
		}

		// Add the serialized component to the components array
		j["components"].push_back(std::move(componentJson));
	}

	outJson = std::move(j);
	return true;
}
