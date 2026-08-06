#pragma once
#include <memory>
#include "Engine/Core/GUID/Guid.h"
#include "nlohmann/json.hpp"

class Actor;

//---------------------------------------------------------------------------
// ActorDeserializer class
// This class creates a detached Actor from one serialized Actor record
// Never register the Actor to a Scene or set its parent-child relationships
//---------------------------------------------------------------------------

class ActorDeserializer
{
public:
	static std::unique_ptr<Actor> DeserializeActorRecord(
		const nlohmann::json& actorJson,
		const Guid& actorGuid
	);
};