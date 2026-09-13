#pragma once
#include "AssetType.h"
#include "Engine/Core/GUID/Guid.h"

enum class AssetChangeKind { Added, Modified, Removed };

// A filesystem/catalog observation, not a claim that asset meaning has changed.
struct AssetChange
{
	AssetChangeKind kind;
	AssetType type;
	Guid guid;
	std::string relativePath;
	friend bool operator==(const AssetChange&, const AssetChange&) = default;
};
