#pragma once
#include "ActorImprintInstanceRecord.h"
#include "Engine/Actor/ActorHandle.h"
#include "nlohmann/json_fwd.hpp"
#include <string>

class SceneBase;

enum class ActorImprintInstanceSerializationErrorCode
{
	None,
	InvalidScene,
	InvalidInstance,
	InvalidDefinition,
	InvalidIdentity,
	InvalidProperty,
	InvalidRecord,
};

struct ActorImprintInstanceSerializationError
{
	ActorImprintInstanceSerializationErrorCode code = ActorImprintInstanceSerializationErrorCode::None;
	LocalObjectId targetLocalObjectId = InvalidLocalObjectId;
	std::string path;
	std::string message;
};

// Generates persistence data from the live Instance and the current immutable default.
// The Registry remains provenance-only; no Override intent is retained at runtime.
class ActorImprintInstanceSerializer
{
public:
	static bool Capture(const SceneBase& scene, ActorHandle root,
		ActorImprintSerializedInstanceRecord& outRecord,
		ActorImprintInstanceSerializationError* outError = nullptr);
	static bool Serialize(const SceneBase& scene, ActorHandle root, nlohmann::json& outJson,
		ActorImprintInstanceSerializationError* outError = nullptr);
};
