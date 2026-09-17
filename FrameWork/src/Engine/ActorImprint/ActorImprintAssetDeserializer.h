#pragma once
#include "ActorImprint.h"
#include <memory>

class AssetReferenceSaveContext;

class ActorImprintAssetDeserializer
{
public:
	// Non-null asset references require a validating catalog context.
	static std::unique_ptr<const ActorImprint> Deserialize(
		const nlohmann::json& json,
		const AssetReferenceSaveContext* assetContext = nullptr);
	static std::unique_ptr<const ActorImprint> Load(
		const std::string& path,
		const AssetReferenceSaveContext* assetContext = nullptr);
};
