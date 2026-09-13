#pragma once
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetManager.h"
#include "UI/AssetDragDropPayload.h"

//------------------------------------------------------------
// AssetPicker
// Draws a editor UI for selecting an asset from the catalog.
//------------------------------------------------------------

namespace AssetPicker
{
	bool TrySelectPayload(const AssetManager& assetManager, AssetType expectedType,
		const EditorAssetDragDropPayload& payload, const Guid& currentAssetId,
		Guid& outSelectedAssetId);
	bool Draw(
		const char* label,
		const AssetManager& assetManager,
		AssetType type,
		const Guid& currentAssetId,
		Guid& outSelectedAssetId
	);
}
