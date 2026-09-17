#pragma once
#include "Engine/Resource/AssetReference.h"
#include "nlohmann/json_fwd.hpp"

class AssetReferenceSaveContext
{
public:
	virtual ~AssetReferenceSaveContext() = default;
	virtual bool Validate(
		const Guid& guid,
		AssetType expectedType) const = 0;
};

class AssetReferenceRestoreContext
{
public:
	virtual ~AssetReferenceRestoreContext() = default;
	virtual bool Resolve(
		const Guid& guid,
		AssetType expectedType) const = 0;
};

// This serializes and deserializes AssetReference values using GUIDs.
class AssetReferenceCodec
{
public:
	bool Serialize(
		const AssetReferenceValue& reference,
		const AssetReferenceSaveContext& context,
		nlohmann::json& outJson) const;

	bool Deserialize(
		const nlohmann::json& json,
		AssetType expectedType,
		AssetReferenceValue& outReference) const;

	bool Resolve(
		AssetReferenceValue& reference,
		const AssetReferenceRestoreContext& context) const;
};
