#pragma once
#include "Engine/Core/GUID/Guid.h"
#include <string>
#include <optional>

//----------------------------------------------------------------
// Meta file is a JSON file that stores GUID for each asset files 
// to ensure that the same asset file always has the same GUID.
// This file is created when an new asset is loaded.
// AssetManager calls these functions to manage .meta files.
//----------------------------------------------------------------

namespace MetaFile
{
	// Trying to load .meta file
	// Returns the Guid if successful, otherwise returns std::nullopt
	std::optional<Guid> TryLoad(const std::string& path);

	// Save .meta file with the given GUID
	void Save(const std::string& path, const Guid& guid);
}