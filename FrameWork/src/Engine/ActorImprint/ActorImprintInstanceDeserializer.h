#pragma once
#include "ActorImprintInstanceRecord.h"
#include "ActorImprintSystem.h"
#include "nlohmann/json_fwd.hpp"
#include <string>

class SceneBase;
class Actor;

enum class ActorImprintInstanceDeserializationErrorCode
{
	None,
	InvalidRecord,
	InvalidScene,
	InvalidAsset,
	InvalidExternalParent,
	MaterializationFailed,
};

struct ActorImprintInstanceDeserializationError
{
	ActorImprintInstanceDeserializationErrorCode code = ActorImprintInstanceDeserializationErrorCode::None;
	LocalObjectId targetLocalObjectId = InvalidLocalObjectId;
	std::string path;
	std::string message;
};

struct ActorImprintPreparedRestore
{
	ActorImprintHandle imprint;
	ActorImprintRestoreInput input;
};

// Converts persistent GUID-based data to a RestoreInstance input. Actor construction
// remains owned by ActorImprintSystem and its Scene transaction.
class ActorImprintInstanceDeserializer
{
public:
	static bool Prepare(const ActorImprintSerializedInstanceRecord& record, SceneBase& scene,
		ActorImprintSystem& system, ActorImprintPreparedRestore& outRestore,
		ActorImprintInstanceDeserializationError* outError = nullptr);
	static bool Deserialize(const nlohmann::json& json, SceneBase& scene,
		ActorImprintSystem& system, ActorImprintPreparedRestore& outRestore,
		ActorImprintInstanceDeserializationError* outError = nullptr);
	static Actor* Restore(const nlohmann::json& json, SceneBase& scene,
		ActorImprintSystem& system, ActorImprintInstanceDeserializationError* outError = nullptr);
};
