#pragma once
#include "Engine/Core/GUID/Guid.h"
#include <string>
#include <optional>

namespace MetaFile
{
	// Trying to load .meta file
	// Returns the Guid if successful, otherwise returns std::nullopt
	std::optional<Guid> TryLoad(const std::string& path);

	// Save .meta file with the given GUID
	void Save(const std::string& path, const Guid& guid);
}