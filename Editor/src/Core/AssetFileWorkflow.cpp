#include "AssetFileWorkflow.h"
#include "Core/AssetCreationPath.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/BuiltinMeshAssets.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Core/GUID/GuidGenerator.h"

#include <Windows.h>
#include <array>
#include <algorithm>
#include <filesystem>
#include <cwchar>

namespace
{

namespace fs = std::filesystem;

// Holds pair of asset data and meta file paths for single asset.
struct FilePair
{
	std::array<fs::path, 3> data;
	std::array<fs::path, 3> meta;
	int dataAt = 0;
	int metaAt = 0;

	// Moves the asset data and meta file to the specified destination index in the arrays.
	bool MoveTo(int destination)
	{
		if (!MoveFileExW(data[dataAt].c_str(), data[destination].c_str(), MOVEFILE_WRITE_THROUGH))
		{
			return false;
		}

		dataAt = destination;

		if (!MoveFileExW(meta[metaAt].c_str(), meta[destination].c_str(), MOVEFILE_WRITE_THROUGH))
		{
			return false;
		}

		metaAt = destination;

		return true;
	}

	// Restores the asset data and meta file to their original locations.
	bool Restore()
	{
		bool success = true;

		if (dataAt != 0)
		{
			if (MoveFileExW(data[dataAt].c_str(), data[0].c_str(), MOVEFILE_WRITE_THROUGH))
			{
				dataAt = 0;
			}
			else
			{
				success = false;
			}
		}

		if (metaAt != 0)
		{
			if (MoveFileExW(meta[metaAt].c_str(), meta[0].c_str(), MOVEFILE_WRITE_THROUGH))
			{
				metaAt = 0;
			}
			else
			{
				success = false;
			}
		}

		return success;
	}
};

bool Reject(FilePair& pair, std::string& error, const char* message)
{
	const bool restored = pair.Restore();
	error = message;

	if (!restored)
	{
		error += " File recovery also failed; preserve the transaction files for recovery.";
	}
	return false;
}

// Prepares the asset data and meta file paths for a transaction.
bool Prepare(
	AssetManager& assets,
	const Guid& guid,
	FilePair& pair,
	AssetType& type,
	std::string& error)
{
	const AssetEntry* entry = assets.GetAssetEntry(guid);

	// Reject if the asset is missing or read-only (built-in).
	if (!entry || FindBuiltinMeshAsset(guid))
	{
		error = "The asset is missing or read-only.";
		return false;
	}

	type = entry->type;

	
	const fs::path relative(entry->relativePath);
	fs::path parent = relative.parent_path();

	if (parent.empty())
	{
		parent = ".";
	}

	// Resolve the directory for the asset's parent folder.
	fs::path directory;

	if (!AssetCreationPath::ResolveDirectory(assets, parent, directory, error))
	{
		return false;
	}

	// Prepare the asset data and meta file paths for the transaction.
	pair.data[0] = directory / relative.filename();
	pair.meta[0] = pair.data[0].wstring() + L".meta";
	const auto metadata = MetaFile::TryLoad(pair.data[0].string());

	if (!metadata || *metadata != guid)
	{
		error = "The .meta identity does not match the asset catalog.";
		return false;
	}

	// Generate a unique temporary filename for the asset data and meta file.
	const std::wstring token = fs::path(GuidGenerator::Generate().ToString()).wstring();
	pair.data[1] = directory / (relative.filename().wstring() + token + L".asset-tmp");
	pair.meta[1] = directory / (relative.filename().wstring() + token + L".assetmeta-tmp");

	return true;
}

bool MakeName(
	std::string_view input,
	const fs::path& extension,
	fs::path& filename,
	std::string& error)
{
	// Convert the input string to a wide string for validation.
	fs::path supplied = fs::u8path(input.begin(), input.end());
	std::wstring name = supplied.native();

	// Name is invalid if it has a parent path or a root path (e.g., "folder/filename" or "/filename").
	const bool invalidPath = supplied.has_parent_path() || supplied.has_root_path();

	// Do not accept the following characters in the filename: < >
	// : " / \ | ? * and control characters (ASCII < 32).
	const bool invalidCharacter =
		std::any_of(name.begin(), name.end(),
		[](wchar_t c)
		{
			return c < 32 || std::wstring_view(L"<>:\"/\\|?*").find(c) != std::wstring_view::npos;
		});

	if (invalidPath || invalidCharacter || name.empty())
	{
		error = "Enter one valid filename.";
		return false;
	}

	// If the supplied name has the correct extension, remove it to get the stem.
	if (_wcsicmp(supplied.extension().c_str(), extension.c_str()) == 0)
	{
		supplied.replace_extension();
		name = supplied.native();
	}

	// Reject names that are empty or end with a dot or space, which are invalid in Windows.
	if (name.empty() || name.back() == L'.' || name.back() == L' ')
	{
		error = "The name must have a stem and must not end in a dot or space.";
		return false;
	}
	
	// Extract the device part of the name (before the first dot).
	const std::wstring device = name.substr(0, name.find(L'.'));

	// Reject names that match reserved device names in Windows.
	static constexpr const wchar_t* reserved[] = {
		L"CON", L"PRN", L"AUX", L"NUL", L"CLOCK$",
		L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
		L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9" };

	for (const wchar_t* value : reserved)
	{
		if (_wcsicmp(device.c_str(), value) == 0)
		{
			error = "The filename is reserved by Windows.";
			return false;
		}
	}

	// Construct the final filename with the correct extension.
	filename = fs::path(name + extension.native());

	// Reject names that are too long when combined with the extension.
	if (filename.native().size() + 5 > 255)
	{
		error = "The filename including its .meta suffix is too long.";
		return false;
	}

	return true;
}

}

bool AssetFileWorkflow::Rename(
	AssetManager& assets,
	const Guid& guid,
	std::string_view newName,
	std::string& outPath,
	std::string& error)
{
	error.clear();
	outPath.clear();

	FilePair pair;
	AssetType type = AssetType::Unknown;

	// Prepare transaction files for the asset to be renamed.
	if (!Prepare(assets, guid, pair, type, error))
	{
		return false;
	}

	// Texture is not supported to rename currently, because model files may reference its path.
	if (type == AssetType::Texture)
	{
		error = "Texture rename is disabled because model files may reference its path.";
		return false;
	}

	fs::path filename;

	// Validate the new name and construct the new filename with the correct extension.
	if (!MakeName(newName, pair.data[0].extension(), filename, error))
	{
		return false;
	}

	// Construct the new asset data and meta file paths based on the new filename.
	pair.data[2] = pair.data[0].parent_path() / filename;
	pair.meta[2] = pair.data[2].wstring() + L".meta";
	outPath = pair.data[2].string();

	if (pair.data[0] == pair.data[2])
	{
		return true;
	}

	// Check for name collisions with existing files in the same directory.
	for (const auto& entry : fs::directory_iterator(pair.data[0].parent_path()))
	{
		const auto existing = entry.path().filename().native();

		const bool collision =
			_wcsicmp(existing.c_str(), filename.c_str()) == 0 ||
			_wcsicmp(existing.c_str(), pair.meta[2].filename().c_str()) == 0;
		
		if (collision)
		{
			const bool ownFile =
				fs::equivalent(entry.path(), pair.data[0]) ||
				fs::equivalent(entry.path(), pair.meta[0]);

			if (!ownFile)
			{
				error = "An asset or .meta file with this name already exists.";
				return false;
			}
		}
	}

	// Move the asset data and meta file to the new paths.
	if (!pair.MoveTo(1) || !pair.MoveTo(2))
	{
		return Reject(pair, error, "Could not rename the asset and its metadata.");
	}

	// Notify the AssetManager of the asset change to update the catalog.
	const std::string relative = pair.data[2].lexically_relative(assets.GetAssetRoot()).generic_string();
	AssetCatalogError catalogError;

	if (!assets.NotifyAssetChanged(relative, &catalogError))
	{
		return Reject(pair, error, "Catalog rejected the rename.");
	}

	return true;
}

bool AssetFileWorkflow::Delete(AssetManager& assets, const Guid& guid, std::string& error)
{
	error.clear();

	FilePair pair;
	AssetType type = AssetType::Unknown;

	// Prepare transaction files for the asset to be deleted.
	if (!Prepare(assets, guid, pair, type, error))
	{
		return false;
	}

	// Reject deletion of ActorImprint assets, which require a reference-aware workflow.
	if (type == AssetType::ActorImprint)
	{
		error = "ActorImprint deletion requires its reference-aware workflow.";
		return false;
	}

	// Move the asset data and meta file to temporary paths for deletion.
	const std::string relative = pair.data[0].lexically_relative(assets.GetAssetRoot()).generic_string();

	if (!pair.MoveTo(1))
	{
		return Reject(pair, error, "Could not stage the asset for deletion.");
	}

	// Notify the AssetManager of the asset change to update the catalog.
	AssetCatalogError catalogError;

	if (!assets.NotifyAssetChanged(relative, &catalogError))
	{
		return Reject(pair, error, "Catalog rejected the deletion.");
	}

	// Attempt to delete the asset data and meta file from the filesystem.
	const bool dataRemoved = DeleteFileW(pair.data[1].c_str()) != FALSE;
	const bool metaRemoved = DeleteFileW(pair.meta[1].c_str()) != FALSE;

	if (!dataRemoved || !metaRemoved)
	{
		error = "Asset deletion committed, but temporary-file cleanup failed.";
	}

	return true;
}