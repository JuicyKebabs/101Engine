#pragma once

#include "Engine/ActorImprint/ActorImprintAssetDeserializer.h"
#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class ActorImprintEditingObjectMap;
class AssetManager;
struct EngineContext;
class SceneBase;

struct ActorImprintEditingSnapshotError
{
	ActorImprintAssetErrorCode code = ActorImprintAssetErrorCode::None;
	LocalObjectId objectId = InvalidLocalObjectId;
	std::string path;
	std::string message;
};

class ActorImprintEditingSnapshot
{
public:
	static std::unique_ptr<const ActorImprint> Capture(const SceneBase& scene,
		const ActorImprintEditingObjectMap& objectMap, const AssetManager& assets,
		nlohmann::json& outJson, ActorImprintEditingSnapshotError* outError = nullptr);
	// Creates the minimum authoring model through the same Scene/ObjectMap and
	// reflection path used by ordinary saves. ET-17 may persist this snapshot as
	// a new asset without maintaining a second hand-written JSON definition.
	static std::unique_ptr<const ActorImprint> CreateDefault(
		const AssetManager& assets, EngineContext& engineContext,
		nlohmann::json& outJson, ActorImprintEditingSnapshotError* outError = nullptr);
};
