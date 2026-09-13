#pragma once

#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetType.h"

#include <type_traits>

inline constexpr const char* EditorAssetDragDropPayloadType = "101ENGINE_ASSET_GUID_TYPE";

struct EditorAssetDragDropPayload
{
	Guid assetGuid;
	AssetType assetType = AssetType::Unknown;
};

static_assert(std::is_trivially_copyable_v<EditorAssetDragDropPayload>);
