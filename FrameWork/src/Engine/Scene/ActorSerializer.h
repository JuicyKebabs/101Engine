#pragma once
#include "nlohmann/json_fwd.hpp"

class Actor;
class SceneBase;

//--------------------------------------------------------------------------------------------
// ActorSerialization class
// This class converts an Actor and its components into a JSON actor record for serialization
// Just creating a JSON record does not perform file I/O, it only prepares the data for saving
//--------------------------------------------------------------------------------------------

class ActorSerializer
{
public:
	static bool SerializeActorRecord(Actor* actor, const SceneBase* scene, nlohmann::json& outJson);
};
