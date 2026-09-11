#pragma once

#include "Engine/Core/Reflection/AssetReferenceCodec.h"

class AssetManager;

class AssetManagerAssetReferenceContext final
	: public AssetReferenceSaveContext,
	  public AssetReferenceRestoreContext
{
public:
	explicit AssetManagerAssetReferenceContext(const AssetManager& assetManager)
		: m_assetManager(assetManager)
	{}

	AssetReferenceCodecResult Validate(
		const Guid& guid,
		AssetType expectedType) const override;

	AssetReferenceCodecResult Resolve(
		const Guid& guid,
		AssetType expectedType) const override;

private:
	AssetReferenceCodecResult Check(
		const Guid& guid,
		AssetType expectedType) const;

	const AssetManager& m_assetManager;
};
