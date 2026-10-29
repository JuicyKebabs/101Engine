#pragma once

#include "Engine/Core/Reflection/AssetReferenceCodec.h"

class AssetManager;

class AssetManagerAssetReferenceContext final
	: public AssetReferenceSaveContext,
	  public AssetReferenceRestoreContext
{
public:
	explicit AssetManagerAssetReferenceContext(const AssetManager& assetManager, bool preserveMissingRenderAssets = false)
        : m_assetManager(assetManager), m_preserveMissingRenderAssets(preserveMissingRenderAssets)
	{}

	bool Validate(
		const Guid& guid,
		AssetType expectedType) const override;

	bool Resolve(
		const Guid& guid,
		AssetType expectedType) const override;

private:
	bool Check(
		const Guid& guid,
		AssetType expectedType) const;

	const AssetManager& m_assetManager;
    bool m_preserveMissingRenderAssets = false;
};
