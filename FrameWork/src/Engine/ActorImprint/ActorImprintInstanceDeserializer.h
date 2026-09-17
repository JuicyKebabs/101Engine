#pragma once
#include "ActorImprintInstanceRecord.h"
#include "ActorImprintSystem.h"
#include "nlohmann/json_fwd.hpp"
#include <string>

class SceneBase;
class Actor;

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
	static bool Prepare(
		const ActorImprintSerializedInstanceRecord& record,
		SceneBase& scene,
		ActorImprintSystem& system,
		ActorImprintPreparedRestore& outRestore);
	static bool Deserialize(
		const nlohmann::json& json,
		SceneBase& scene,
		ActorImprintSystem& system,
		ActorImprintPreparedRestore& outRestore);
	static Actor* Restore(
		const nlohmann::json& json,
		SceneBase& scene,
		ActorImprintSystem& system);
};
