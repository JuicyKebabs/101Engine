#include "AssetCreationPath.h"

#include "Engine/Resource/AssetManager.h"
#include <Windows.h>
#include <algorithm>
#include <cwchar>

namespace
{

namespace fs = std::filesystem;

bool IsWithin(const fs::path& root, const fs::path& directory)
{
	auto child = directory.begin();

	for (const fs::path& part : root)
	{
		if (child == directory.end())
		{
			return false;
		}

		if (_wcsicmp(part.c_str(), child->c_str()) != 0)
		{
			return false;
		}

		child++;
	}

	return true;
}

}

bool AssetCreationPath::ResolveDirectory(
	const AssetManager& assetManager,
	const std::filesystem::path& relativeDirectory,
	std::filesystem::path& outDirectory,
	std::string& outError)
{
	outDirectory.clear();
	outError.clear();

	try
	{
		// "." means the asset root. The directory must already exist.
		const bool invalidRoot = assetManager.GetAssetRoot().empty();
		// Reject rooted paths.
		const bool rootedPath = relativeDirectory.has_root_path();
		// Reject embedded nulls.
		const bool invalidText = relativeDirectory.native().find(L'\0') != std::wstring::npos;

		if (invalidRoot || rootedPath || invalidText)
		{
			outError = "Expected a directory relative to the asset root.";
			return false;
		}

		if (relativeDirectory.empty())
		{
			outError = "No destination directory was selected.";
			return false;
		}

		// Reject parent-directory components. The resolved directory must be a child of the asset root.
		for (const fs::path& part : relativeDirectory)
		{
			if (part == "..")
			{
				outError = "Parent-directory components are not allowed.";
				return false;
			}
		}

		// Check if the absolute directory is directory.
		const fs::path root = fs::absolute(assetManager.GetAssetRoot()).lexically_normal();
		const fs::path directory = (root / relativeDirectory).lexically_normal();

		if (!fs::is_directory(directory))
		{
			outError = "The destination directory no longer exists.";
			return false;
		}

		// Check if the absolute directory is within the asset root.
		const fs::path resolvedRoot = fs::canonical(root);
		const fs::path resolvedDirectory = fs::canonical(directory);

		if (!IsWithin(resolvedRoot, resolvedDirectory))
		{
			outError = "The destination resolves outside the asset root.";
			return false;
		}

		// Check if the absolute directory is a reparse point (directory link).
		fs::path current = root;

		for (const fs::path& part : relativeDirectory.lexically_normal())
		{
			if (part == "." || part.empty())
			{
				continue;
			}

			current /= part;
			const DWORD attributes = GetFileAttributesW(current.c_str());

			if (attributes == INVALID_FILE_ATTRIBUTES)
			{
				outError = "Could not inspect the destination directory.";
				return false;
			}

			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			{
				outError = "Directory links are not supported as creation destinations.";
				return false;
			}
		}

		outDirectory = directory;
		return true;
	}
	catch (const std::exception& error)
	{
		outError = error.what();
		return false;
	}
}

bool AssetCreationPath::CheckAvailable(
	const std::filesystem::path& directory,
	const std::filesystem::path& filename,
	std::string& outError)
{
	outError.clear();

	try
	{
		const std::wstring name = filename.native();
		const bool invalidComponent = filename.empty() || filename.has_parent_path();

		const bool controlCharacter = std::any_of(
			name.begin(), name.end(),
			[](wchar_t character)
			{
				return character < 32;
			});

		if (invalidComponent || controlCharacter)
		{
			outError = "Expected one valid filename.";
			return false;
		}

		// Check for collisions with either the data file or its .meta sidecar.
		const std::wstring metadataName = name + L".meta";

		for (const fs::directory_entry& entry : fs::directory_iterator(directory))
		{
			const std::wstring existing = entry.path().filename().native();
			const bool dataCollision = _wcsicmp(existing.c_str(), name.c_str()) == 0;
			const bool metadataCollision = _wcsicmp(existing.c_str(), metadataName.c_str()) == 0;

			if (dataCollision || metadataCollision)
			{
				outError = "An asset or .meta file with this name already exists.";
				return false;
			}
		}

		return true;
	}
	catch (const std::exception& error)
	{
		outError = error.what();
		return false;
	}
}