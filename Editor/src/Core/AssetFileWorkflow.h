#pragma once

#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetType.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

class AssetManager;

namespace AssetFileWorkflow
{
	bool CreateGenerated(
		AssetManager& assetManager,
		const std::filesystem::path& relativeDirectory,
		std::string_view name,
		const std::filesystem::path& extension,
		std::string_view bytes,
		Guid& outGuid,
		std::string& diagnostic);

	// Copy supported files with fresh GUIDs and refresh the catalog once.
	bool Import(
		AssetManager& assetManager,
		const std::filesystem::path& relativeDirectory,
		std::span<const std::filesystem::path> sources,
		std::string& diagnostic);

	bool Rename(
		AssetManager& assetManager,
		const Guid& guid,
		std::string_view name,
		std::string& outPath,
		std::string& error);

	bool SupportsMove(AssetType type);

	bool Move(
		AssetManager& assetManager,
		const Guid& guid,
		const std::filesystem::path& relativeDirectory,
		std::string& outPath,
		std::string& error);

	bool Delete(
		AssetManager& assetManager,
		const Guid& guid,
		std::string& error);
}
