#pragma once
#include "ActorImprint.h"
#include <memory>

class AssetReferenceSaveContext;

enum class ActorImprintAssetErrorCode
{
	None,
	IoError,
	InvalidJson,
	UnsupportedVersion,
	InvalidSchema,
	InvalidRevision,
	InvalidObjectGraph,
	InvalidComponent,
	InvalidProperty,
};

struct ActorImprintAssetError
{
	ActorImprintAssetErrorCode code = ActorImprintAssetErrorCode::None;
	std::string path; // JSON Pointer, or file path for I/O failure.
	std::string message;
};

class ActorImprintAssetDeserializer
{
public:
	// Non-null asset references require a validating catalog context.
	static std::unique_ptr<const ActorImprint> Deserialize(const nlohmann::json& json,
		const AssetReferenceSaveContext* assetContext = nullptr, ActorImprintAssetError* outError = nullptr);
	static std::unique_ptr<const ActorImprint> Load(const std::string& path,
		const AssetReferenceSaveContext* assetContext = nullptr, ActorImprintAssetError* outError = nullptr);
};
