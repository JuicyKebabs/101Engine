#include "AssetReferenceCodec.h"
#include "nlohmann/json.hpp"

AssetReferenceCodecResult AssetReferenceCodec::Serialize(
	const AssetReferenceValue& reference,
	const AssetReferenceSaveContext& context,
	nlohmann::json& outJson) const
{
	if (!reference.HasValue())
	{
		outJson = nullptr;
		return AssetReferenceCodecResult::Success;
	}

	if (reference.expectedType == AssetType::Unknown)
	{
		return AssetReferenceCodecResult::AssetTypeMismatch;
	}

	const AssetReferenceCodecResult result = context.Validate(reference.guid, reference.expectedType);
	if (result != AssetReferenceCodecResult::Success)
	{
		return result;
	}

	// Convert GUID to string for JSON serialization
	outJson = reference.guid.ToString();
	return AssetReferenceCodecResult::Success;
}

AssetReferenceCodecResult AssetReferenceCodec::Deserialize(
	const nlohmann::json& json,
	AssetType expectedType,
	AssetReferenceValue& outReference) const
{
	if (expectedType == AssetType::Unknown)
	{
		return AssetReferenceCodecResult::AssetTypeMismatch;
	}

	// Null means no asset is selected (No need to resolve)
	if (json.is_null())
	{
		outReference = { {}, expectedType, false };
		return AssetReferenceCodecResult::Success;
	}

	// Only accept string type for GUID representation,
	if (!json.is_string()) return AssetReferenceCodecResult::InvalidJsonType;

	Guid guid;
	if (!Guid::TryParse(json.get<std::string>(), guid) || !guid.IsValid())
	{
		return AssetReferenceCodecResult::InvalidGuid;
	}

	// Save the GUID with given expected type,.
	// But not resolved yet. Just proved that the GUID is valid
	// Resolve asset after it is confirmed that the AssetManager finished loading all assets.
	outReference = { guid, expectedType, false };
	return AssetReferenceCodecResult::Success;
}

AssetReferenceCodecResult AssetReferenceCodec::Resolve(
	AssetReferenceValue& reference,
	const AssetReferenceRestoreContext& context) const
{
	if (!reference.HasValue()) return AssetReferenceCodecResult::Success;	// empty reference is considered resolved (No need to resolve)
	if (reference.expectedType == AssetType::Unknown)
	{// This reference is made from an concrete AssetReference<T> with never registered asset type T, so it can never be resolved.
		return AssetReferenceCodecResult::AssetTypeMismatch;
	}

	// Resolve the reference with AssetManager in the context.
	const AssetReferenceCodecResult result = context.Resolve(reference.guid, reference.expectedType);

	if (result != AssetReferenceCodecResult::Success)
	{
		return result;
	}

	reference.resolved = true;
	return AssetReferenceCodecResult::Success;
}
