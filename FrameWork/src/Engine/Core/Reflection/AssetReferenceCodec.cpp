#include "AssetReferenceCodec.h"
#include "Engine/Core/Debug/Debug.h"
#include "nlohmann/json.hpp"

bool AssetReferenceCodec::Serialize(
	const AssetReferenceValue& reference,
	const AssetReferenceSaveContext& context,
	nlohmann::json& outJson) const
{
	if (!reference.HasValue())
	{
		outJson = nullptr;
		return true;
	}

	if (reference.expectedType == AssetType::Unknown)
	{
		DBG("Asset reference: AssetTypeMismatch.");
		return false;
	}

	const bool result = context.Validate(reference.guid, reference.expectedType);

	if (!result)
	{
		return result;
	}

	// Convert GUID to string for JSON serialization
	outJson = reference.guid.ToString();
	return true;
}

bool AssetReferenceCodec::Deserialize(
	const nlohmann::json& json,
	AssetType expectedType,
	AssetReferenceValue& outReference) const
{
	if (expectedType == AssetType::Unknown)
	{
		DBG("Asset reference: AssetTypeMismatch.");
		return false;
	}

	// Null means no asset is selected (No need to resolve)
	if (json.is_null())
	{
		outReference = { {}, expectedType, false };
		return true;
	}

	// Only accept string type for GUID representation,
	if (!json.is_string())
	{
		DBG("Asset reference: InvalidJsonType.");
		return false;
	}

	Guid guid;
	const std::string text = json.get<std::string>();

	if (text.find('\0') != std::string::npos || !Guid::TryParse(text, guid) || !guid.IsValid())
	{
		DBG("Asset reference: InvalidGuid.");
		return false;
	}

	// Save the GUID with given expected type,.
	// But not resolved yet. Just proved that the GUID is valid
	// Resolve asset after it is confirmed that the AssetManager finished loading all assets.
	outReference = { guid, expectedType, false };
	return true;
}

bool AssetReferenceCodec::Resolve(
	AssetReferenceValue& reference,
	const AssetReferenceRestoreContext& context) const
{
	if (!reference.HasValue())
	{
		return true; // empty reference is considered resolved (No need to resolve)
	}

	if (reference.expectedType == AssetType::Unknown)
	{// This reference is made from an concrete AssetReference<T> with never registered asset type T, so it can never be resolved.
		DBG("Asset reference: AssetTypeMismatch.");
		return false;

	}

	// Resolve the reference with AssetManager in the context.
	const bool result = context.Resolve(reference.guid, reference.expectedType);

	if (!result)
	{
		return result;
	}

	reference.resolved = true;
	return true;
}
