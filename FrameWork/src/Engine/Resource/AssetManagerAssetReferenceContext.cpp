#include "AssetManagerAssetReferenceContext.h"
#include "Engine/Resource/AssetManager.h"

AssetReferenceCodecResult AssetManagerAssetReferenceContext::Validate(
	const Guid& guid,
	AssetType expectedType) const
{
	return Check(guid, expectedType);
}

AssetReferenceCodecResult AssetManagerAssetReferenceContext::Resolve(
	const Guid& guid,
	AssetType expectedType) const
{
	return Check(guid, expectedType);
}

AssetReferenceCodecResult AssetManagerAssetReferenceContext::Check(
	const Guid& guid,
	AssetType expectedType) const
{
	if (!guid.IsValid()) return AssetReferenceCodecResult::InvalidGuid;

	// Return the result based on the asset entry's existence and type match.
	const AssetEntry* entry = m_assetManager.GetAssetEntry(guid);
	if (!entry) return AssetReferenceCodecResult::AssetNotFound;
	if (entry->type != expectedType) return AssetReferenceCodecResult::AssetTypeMismatch;
	return AssetReferenceCodecResult::Success;
}
