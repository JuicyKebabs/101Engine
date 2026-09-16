#pragma once
#include <memory>
#include <string>
#include "Engine/Core/GUID/Guid.h"
#include "nlohmann/json.hpp"
#include "Engine/Core/Reflection/ReflectionSerialization.h"

class Actor;

//---------------------------------------------------------------------------
// ActorDeserializer class
// This class creates a detached Actor from one serialized Actor record
// Never register the Actor to a Scene or set its parent-child relationships
//---------------------------------------------------------------------------

struct ActorDeserializationError
{
	std::string path;
	std::string message;
};

struct ActorDeserializationOptions
{
	// How to behave when encountering unknown properties in the Actor JSON.
	UnknownPropertyPolicy unknownComponentPropertyPolicy = UnknownPropertyPolicy::Reject;
};

class ActorDeserializer
{
public:
	static std::unique_ptr<Actor> DeserializeActorRecord(
		const nlohmann::json& actorJson,
		const Guid& actorGuid,
		ActorDeserializationError* outError = nullptr
	);

	static std::unique_ptr<Actor> DeserializeActorRecord(
		const nlohmann::json& actorJson,
		const Guid& actorGuid,
		ActorDeserializationOptions options,
		ActorDeserializationError* outError = nullptr
	);
};
