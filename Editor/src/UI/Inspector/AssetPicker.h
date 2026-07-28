#pragma once
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetManager.h"

//------------------------------------------------------------
// AssetPicker
// Draws a editor UI for selecting an asset from the catalog.
//------------------------------------------------------------

namespace AssetPicker
{
	bool Draw(
		const char* label,
		const AssetManager& assetManager,
		AssetType type,
		const Guid& currentAssetId,
		Guid& outSelectedAssetId
	);
}