#include "ActorDeserializer.h"
#include "ComponentDeserializer.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Core/Debug/Debug.h"

using json = nlohmann::json;

std::unique_ptr<Actor> ActorDeserializer::DeserializeActorRecord(
	const json& actorJson,
	const Guid& actorGuid)
{
	// Validate the input parameters
	if (!actorJson.is_object())
	{
		DBG("ActorDeserializer::DeserializeActorRecord: Actor record must be an object.");
		return nullptr;
	}

	if (!actorGuid.IsValid())
	{
		DBG("ActorDeserializer::DeserializeActorRecord: Actor Guid is invalid.");
		return nullptr;
	}

	// Build the Actor::InitDesc from the JSON data
	Actor::InitDesc desc;
	desc.name = actorJson.value("name", "Actor");
	desc.isActive = actorJson.value("is_active", true);

	const std::string tagName = actorJson.value("tag", "None");
	desc.tag = tagName.empty() ? TAG_NONE : TagRegistry::Get().GetId(tagName);

	// Create a new Actor instance using the ActorFactory
	std::unique_ptr<Actor> actor = ActorFactory::RestoreActorShell(desc, actorGuid);

	if (!actor)
	{
		DBG(
			"ActorDeserializer::DeserializeActorRecord: Failed to create Actor '%s'.",
			desc.name.c_str());
		return nullptr;
	}

	// Deserialize and attach components to the Actor

	if (!actorJson.contains("components") || !actorJson["components"].is_array())
	{
		DBG("ActorDeserializer::DeserializeActorRecord: Actor '%s' has no components array.", desc.name.c_str());
		return nullptr;
	}

	bool hasTransform = false;

	for (const json& componentRecord : actorJson["components"])
	{
		std::unique_ptr<Component> component = ComponentDeserializer::DeserializeRecord(componentRecord);

		if (!component)
		{
			DBG(
				"ActorDeserializer::DeserializeActorRecord: Failed to deserialize a component for Actor '%s'.",
				desc.name.c_str()
			);
			return nullptr;
		}

		std::string componentTypeName = ComponentRegistry::Get().GetNameByTypeIndex(typeid(*component));

		// Check if the component type is a Transform or RectTransform
		const bool isTransformComponent = (componentTypeName == "Transform") || (componentTypeName == "RectTransform");

		if (isTransformComponent)
		{
			// Check if the transform is duplicate (only one Transform-family component is allowed per Actor)
			if (hasTransform)
			{
				DBG("ActorDeserializer::DeserializeActorRecord: Actor '%s' contains multiple Transform-family components.", desc.name.c_str());
				return nullptr;
			}
		}

		// Attach the component to the Actor
		if (!actor->AddComponent(std::move(component)))
		{
			DBG("ActorDeserializer::DeserializeActorRecord: Failed to add component '%s' to Actor '%s'.", componentTypeName.c_str(), desc.name.c_str());
			return nullptr;
		}

		// Mark that the Actor has a Transform-family component
		if (isTransformComponent) hasTransform = true;
	}

	// Check if the actor has a Transform-family component
	if (!hasTransform)
	{
		DBG("ActorDeserializer::DeserializeActorRecord: Actor '%s' is missing a Transform-family component.", desc.name.c_str());
		return nullptr;
	}

	return actor;
}
