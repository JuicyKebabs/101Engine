#pragma once
#include "Engine/Resource/AssetReference.h"
#include "nlohmann/json_fwd.hpp"

enum class AssetReferenceCodecResult
{
	Success,
	InvalidJsonType,
	InvalidGuid,
	AssetNotFound,
	AssetTypeMismatch,
};

class AssetReferenceSaveContext
{
public:
	virtual ~AssetReferenceSaveContext() = default;
	virtual AssetReferenceCodecResult Validate(
		const Guid& guid,
		AssetType expectedType) const = 0;
};

class AssetReferenceRestoreContext
{
public:
	virtual ~AssetReferenceRestoreContext() = default;
	virtual AssetReferenceCodecResult Resolve(
		const Guid& guid,
		AssetType expectedType) const = 0;
};


// This serializes and deserializes AssetReference values using GUIDs.
class AssetReferenceCodec
{
public:
	AssetReferenceCodecResult Serialize(
		const AssetReferenceValue& reference,
		const AssetReferenceSaveContext& context,
		nlohmann::json& outJson) const;

	AssetReferenceCodecResult Deserialize(
		const nlohmann::json& json,
		AssetType expectedType,
		AssetReferenceValue& outReference) const;

	AssetReferenceCodecResult Resolve(
		AssetReferenceValue& reference,
		const AssetReferenceRestoreContext& context) const;
};
