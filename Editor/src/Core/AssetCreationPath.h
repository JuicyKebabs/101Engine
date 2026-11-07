#pragma once

#include <filesystem>
#include <string>

class AssetManager;

namespace AssetCreationPath
{
	// "." means the asset root. The directory must already exist.
	bool ResolveDirectory(
		const AssetManager& assetManager,
		const std::filesystem::path& relativeDirectory,
		std::filesystem::path& outDirectory,
		std::string& outError);

	// Reject collisions with either the data file or its .meta sidecar.
	bool CheckAvailable(
		const std::filesystem::path& directory,
		const std::filesystem::path& filename,
		std::string& outError);
}