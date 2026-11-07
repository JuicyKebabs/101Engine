#pragma once

#include "Engine/Core/GUID/Guid.h"

#include <string>
#include <string_view>

class AssetManager;

namespace AssetFileWorkflow
{
	bool Rename(
		AssetManager& assets,
		const Guid& guid,
		std::string_view name,
		std::string& outPath,
		std::string& error);

	bool Delete(
		AssetManager& assets,
		const Guid& guid,
		std::string& error);
}