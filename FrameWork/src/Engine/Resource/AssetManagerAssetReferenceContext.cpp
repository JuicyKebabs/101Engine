#include "AssetManagerAssetReferenceContext.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Resource/AssetManager.h"

bool AssetManagerAssetReferenceContext::Validate(
	const Guid& guid,
	AssetType expectedType) const
{
	return Check(guid, expectedType);
}

bool AssetManagerAssetReferenceContext::Resolve(
	const Guid& guid,
	AssetType expectedType) const
{
	return Check(guid, expectedType);
}

bool AssetManagerAssetReferenceContext::Check(
	const Guid& guid,
	AssetType expectedType) const
{
	if (!guid.IsValid())
	{
		DBG("Asset reference: InvalidGuid.");
		return false;
	}

	// Return the result based on the asset entry's existence and type match.
	const AssetEntry* entry = m_assetManager.GetAssetEntry(guid);

	if (!entry)
	{
		DBG("Asset reference: AssetNotFound.");
		return false;
	}

	if (entry->type != expectedType)
	{
		DBG("Asset reference: AssetTypeMismatch.");
		return false;
	}

	return true;
}
