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
	const Guid& actorGuid,
	ActorDeserializationError* outError)
{
	return DeserializeActorRecord(
		actorJson,
		actorGuid,
		{},
		outError);
}

std::unique_ptr<Actor> ActorDeserializer::DeserializeActorRecord(
	const json& actorJson,
	const Guid& actorGuid,
	ActorDeserializationOptions options,
	ActorDeserializationError* outError)
{
	if (outError)
	{
		*outError = {};
	}

	const auto Fail = [&](std::string path, std::string message) -> std::unique_ptr<Actor>
	{
		if (outError)
		{
			*outError = {std::move(path), std::move(message)};
		}

		return nullptr;
	};

	// Validate the input parameters
	if (!actorJson.is_object())
	{
		DBG("ActorDeserializer::DeserializeActorRecord: Actor record must be an object.");
		return Fail({}, "Actor record must be an object.");
	}

	if (!actorGuid.IsValid())
	{
		DBG("ActorDeserializer::DeserializeActorRecord: Actor Guid is invalid.");
		return Fail("/actorId", "Actor GUID must be nonzero.");
	}

	if (actorJson.contains("name") && !actorJson["name"].is_string())
	{
		return Fail("/name", "Actor name must be a string.");
	}

	if (actorJson.contains("is_active") && !actorJson["is_active"].is_boolean())
	{
		return Fail("/is_active", "Actor active state must be a boolean.");
	}

	if (actorJson.contains("tag") && !actorJson["tag"].is_string())
	{
		return Fail("/tag", "Actor tag must be a string.");
	}

	// Build the Actor::InitDesc from the JSON data
	Actor::InitDesc desc;
	desc.name = actorJson.value("name", "Actor");
	desc.isActive = actorJson.value("is_active", true);

	const std::string tagName = actorJson.value("tag", "None");
	desc.tag = tagName.empty() || tagName == "None" ? TAG_NONE : TagRegistry::Get().GetId(tagName);

	// Create a new Actor instance using the ActorFactory
	std::unique_ptr<Actor> actor = ActorFactory::RestoreActorShell(desc, actorGuid);

	if (!actor)
	{
		DBG(
			"ActorDeserializer::DeserializeActorRecord: Failed to create Actor '%s'.",
			desc.name.c_str());
		return Fail({}, "ActorFactory failed to create an Actor shell.");
	}

	// Deserialize and attach components to the Actor

	if (!actorJson.contains("components") || !actorJson["components"].is_array())
	{
		DBG("ActorDeserializer::DeserializeActorRecord: Actor '%s' has no components array.", desc.name.c_str());
		return Fail("/components", "Actor components must be an array.");
	}

	bool hasTransform = false;

	for (std::size_t componentIndex = 0; componentIndex < actorJson["components"].size(); ++componentIndex)
	{
		const json& componentRecord = actorJson["components"][componentIndex];
		ComponentDeserializationError componentError;
		ComponentRestoreOptions restoreOptions;
		restoreOptions.unknownPropertyPolicy = options.unknownComponentPropertyPolicy;
		std::unique_ptr<Component> component =
			ComponentDeserializer::DeserializeRecord(componentRecord, restoreOptions, &componentError);

		if (!component)
		{
			DBG(
				"ActorDeserializer::DeserializeActorRecord: Failed to deserialize a component for Actor '%s'.",
				desc.name.c_str()
			);
			const std::string failureMessage = componentError.message.empty()
				? "Component deserialization failed."
				: componentError.message;
			return Fail("/components/" + std::to_string(componentIndex) + componentError.path,
				failureMessage);
		}

		std::string componentTypeName = ComponentRegistry::Get().GetNameByTypeIndex(typeid(*component));

		// Check if the component type is a Transform or RectTransform
		const bool isTransformComponent = (componentTypeName == "Transform") || (componentTypeName == "RectTransform");

		if (isTransformComponent)
		{
			// Check if the transform is duplicate (only one Transform-family component is allowed per Actor)
			if (hasTransform)
			{
				DBG("ActorDeserializer::DeserializeActorRecord: Actor '%s' contains "
					"multiple Transform-family components.",
					desc.name.c_str());
				return Fail("/components/" + std::to_string(componentIndex),
					"Actor contains multiple Transform-family components.");
			}
		}

		// Attach the component to the Actor
		if (!actor->AddComponent(std::move(component)))
		{
			DBG("ActorDeserializer::DeserializeActorRecord: Failed to add component '%s' to Actor '%s'.",
				componentTypeName.c_str(), desc.name.c_str());
			return Fail("/components/" + std::to_string(componentIndex), "Actor rejected the Component configuration.");
		}

		// Mark that the Actor has a Transform-family component
		if (isTransformComponent)
		{
			hasTransform = true;
		}
	}

	// Check if the actor has a Transform-family component
	if (!hasTransform)
	{
		DBG("ActorDeserializer::DeserializeActorRecord: Actor '%s' is missing a Transform-family component.",
			desc.name.c_str());
		return Fail("/components", "Actor requires exactly one Transform-family component.");
	}

	return actor;
}
