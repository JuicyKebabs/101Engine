#include "AssetFileWorkflow.h"
#include "Core/AssetCreationPath.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/BuiltinMeshAssets.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/String/StringEncoding.h"

#include <Windows.h>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cwchar>
#include <utility>

namespace
{

namespace fs = std::filesystem;

template<class WriteStagedData>
bool PublishAssetFile(
	const fs::path& destination,
	const Guid& guid,
	WriteStagedData writeStagedData,
	std::string& diagnostic)
{
	const fs::path staging = destination.parent_path() / (guid.ToString() + ".asset-tmp");
	const fs::path stagedData = staging / L"data.asset-tmp";
	const fs::path stagedMeta = stagedData.wstring() + L".meta";
	const fs::path finalMeta = destination.wstring() + L".meta";
	const std::string metadataBase = stagedData.string();

	if (fs::path(destination.string()) != destination || fs::path(metadataBase) != stagedData)
	{
		diagnostic = "The destination path cannot be represented in the asset catalog.";
		return false;
	}

	std::error_code error;

	if (!fs::create_directory(staging, error))
	{
		diagnostic = "Could not create an asset staging directory.";
		return false;
	}

	bool metaPublished = false;
	bool dataPublished = false;

	try
	{
		if (!writeStagedData(stagedData, diagnostic))
		{
			// The writer has supplied the diagnostic.
		}
		else if (!MetaFile::Save(metadataBase, guid, MetaFile::WriteMode::CreateNew))
		{
			diagnostic = "Could not write asset metadata.";
		}
		else
		{
			metaPublished = MoveFileExW(stagedMeta.c_str(), finalMeta.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
			dataPublished = metaPublished &&
							MoveFileExW(stagedData.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
			
			if (!dataPublished)
			{
				diagnostic =
					"Could not publish the asset and its metadata (Win32 error " +
					std::to_string(GetLastError()) + ").";
			}
		}
	}
	catch (const std::exception& exception)
	{
		diagnostic = exception.what();
	}

	const auto remove = [&diagnostic](const fs::path& path)
	{
		std::error_code cleanupError;
		fs::remove(path, cleanupError);

		if (cleanupError)
		{
			diagnostic += "\nCould not clean up " + StringEncoding::WideToUtf8(path.native()) + ".";
		}
	};

	if (metaPublished && !dataPublished)
	{
		remove(finalMeta);
	}

	remove(stagedData);
	remove(stagedMeta);
	remove(staging);

	return dataPublished;
}

bool PublishImportedFile(
	const fs::path& source,
	const fs::path& destination,
	std::string& diagnostic)
{
	const Guid guid = GuidGenerator::Generate();

	if (!guid.IsValid())
	{
		diagnostic = "Could not generate an asset GUID.";
		return false;
	}

	return PublishAssetFile(destination, guid,
		[&source](const fs::path& stagedData, std::string& error)
		{
			if (!CopyFileW(source.c_str(), stagedData.c_str(), TRUE))
			{
				error = "Could not copy the file (Win32 error " + std::to_string(GetLastError()) + ").";
				return false;
			}

			return true;
		}, diagnostic);
}

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
	AssetManager& assetManager,
	const Guid& guid,
	FilePair& pair,
	AssetType& type,
	std::string& error)
{
	const AssetEntry* entry = assetManager.GetAssetEntry(guid);

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

	if (!AssetCreationPath::ResolveDirectory(assetManager, parent, directory, error))
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

bool AssetFileWorkflow::CreateGenerated(
	AssetManager& assetManager,
	const fs::path& relativeDirectory,
	std::string_view name,
	const fs::path& extension,
	std::string_view bytes,
	Guid& outGuid,
	std::string& diagnostic)
{
	outGuid = {};
	diagnostic.clear();

	try
	{
		if (AssetManager::DetermineAssetType(extension.string()) == AssetType::Unknown)
		{
			diagnostic = "The asset extension is not registered.";
			return false;
		}

		fs::path directory;
		fs::path filename;

		if (!AssetCreationPath::ResolveDirectory(assetManager, relativeDirectory, directory, diagnostic) ||
			!MakeName(name, extension, filename, diagnostic) ||
			!AssetCreationPath::CheckAvailable(directory, filename, diagnostic))
		{
			return false;
		}

		const fs::path destination = directory / filename;
		const fs::path metadata = destination.wstring() + L".meta";
		const std::string relativePath =
			destination.lexically_relative(assetManager.GetAssetRoot()).generic_string();
		const Guid guid = GuidGenerator::Generate();

		if (!guid.IsValid())
		{
			diagnostic = "Could not generate an asset GUID.";
			return false;
		}

		if (!PublishAssetFile(destination, guid,
			[bytes](const fs::path& stagedData, std::string& error)
			{
				std::ofstream output(stagedData, std::ios::binary);
				output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
				output.close();

				if (!output)
				{
					error = "Could not write the staged asset.";
					return false;
				}

				return true;
			}, diagnostic))
		{
			return false;
		}

		try
		{
			AssetCatalogError catalogError;

			if (assetManager.NotifyAssetChanged(relativePath, &catalogError))
			{
				outGuid = guid;
				return true;
			}

			diagnostic = "Asset catalog update failed: " + catalogError.message;
		}
		catch (const std::exception& exception)
		{
			diagnostic = exception.what();
		}

		// Remove the asset before its metadata to preserve identity if removal fails.
		std::error_code cleanupError;
		fs::remove(destination, cleanupError);

		if (!cleanupError)
		{
			fs::remove(metadata, cleanupError);
		}

		if (cleanupError)
		{
			diagnostic += " Could not fully remove the published asset and metadata.";
		}

		return false;
	}
	catch (const std::exception& exception)
	{
		diagnostic = exception.what();
		return false;
	}
}

bool AssetFileWorkflow::Import(
	AssetManager& assetManager,
	const fs::path& relativeDirectory,
	std::span<const fs::path> sources,
	std::string& diagnostic)
{
	fs::path directory;

	if (!AssetCreationPath::ResolveDirectory(assetManager, relativeDirectory, directory, diagnostic))
	{
		return false;
	}

	std::size_t copied = 0;

	for (const fs::path& source : sources)
	{
		std::string error;
		try
		{
			const fs::path filename = source.filename();

			if (!source.is_absolute() || !fs::is_regular_file(source))
			{
				error = "Expected a regular file.";
			}
			else if (filename.native().find_first_of(L"<>:\"/\\|?*") != std::wstring::npos ||
					 filename.native().ends_with(L'.') || filename.native().ends_with(L' '))
			{
				error = "The file name is not valid in the destination.";
			}
			// Reject files with unsupported extensions based on the AssetManager's supported asset types.
			else if (AssetManager::DetermineAssetType(filename.extension().string()) == AssetType::Unknown)
			{
				error = "Unsupported asset file extension.";
			}
			// Check if the destination path is available and publish the imported file with a new GUID.
			else if (AssetCreationPath::CheckAvailable(directory, filename, error) &&
					 PublishImportedFile(source, directory / filename, error))
			{
				++copied;
			}
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
		}

		if (!error.empty())
		{
			diagnostic += StringEncoding::WideToUtf8(source.filename().native()) + ": " + error + "\n";
		}
	}

	if (copied == 0)
	{
		return false;
	}

	AssetCatalogError catalogError;

	// Refresh the AssetManager's catalog to recognize the newly imported assets.
	if (!assetManager.Refresh(&catalogError))
	{
		diagnostic += 
			std::to_string(copied) + " file(s) copied, but the asset catalog could not be updated.\n" +
			StringEncoding::WideToUtf8(fs::path(catalogError.path).native()) + ": " + catalogError.message;
		
		return false;
	}

	return true;
}

bool AssetFileWorkflow::Rename(
	AssetManager& assetManager,
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
	if (!Prepare(assetManager, guid, pair, type, error))
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
	const std::string relative = pair.data[2].lexically_relative(assetManager.GetAssetRoot()).generic_string();
	AssetCatalogError catalogError;

	if (!assetManager.NotifyAssetChanged(relative, &catalogError))
	{
		return Reject(pair, error, "Catalog rejected the rename.");
	}

	return true;
}

bool AssetFileWorkflow::SupportsMove(AssetType type)
{
	return type == AssetType::Scene ||
		type == AssetType::ActorImprint ||
		type == AssetType::Audio;
}

bool AssetFileWorkflow::Move(
	AssetManager& assetManager,
	const Guid& guid,
	const fs::path& relativeDirectory,
	std::string& outPath,
	std::string& error)
{
	outPath.clear();
	error.clear();

	FilePair pair;
	AssetType type = AssetType::Unknown;
	if (!Prepare(assetManager, guid, pair, type, error))
	{
		return false;
	}

	if (!SupportsMove(type))
	{
		error = "Moving this asset type is disabled.";
		return false;
	}

	fs::path directory;
	if (!AssetCreationPath::ResolveDirectory(assetManager, relativeDirectory, directory, error))
	{
		return false;
	}

	if (fs::equivalent(directory, pair.data[0].parent_path()))
	{
		error = "The asset is already in that folder.";
		return false;
	}

	const fs::path filename = pair.data[0].filename();
	if (!AssetCreationPath::CheckAvailable(directory, filename, error))
	{
		return false;
	}

	pair.data[2] = directory / filename;
	pair.meta[2] = pair.data[2].wstring() + L".meta";

	std::string absolutePath = pair.data[2].string();
	if (fs::path(absolutePath) != pair.data[2])
	{
		error = "The destination path cannot be represented in the asset catalog.";
		return false;
	}

	const std::string relativePath =
		pair.data[2].lexically_relative(assetManager.GetAssetRoot()).generic_string();

	if (!pair.MoveTo(2))
	{
		return Reject(pair, error, "Could not move the asset and its metadata.");
	}

	AssetCatalogError catalogError;
	try
	{
		if (assetManager.NotifyAssetChanged(relativePath, &catalogError))
		{
			outPath = std::move(absolutePath);
			return true;
		}
	}
	catch (const std::exception& exception)
	{
		return Reject(pair, error, exception.what());
	}

	return Reject(pair, error, catalogError.message.c_str());
}

bool AssetFileWorkflow::Delete(AssetManager& assetManager, const Guid& guid, std::string& error)
{
	error.clear();

	FilePair pair;
	AssetType type = AssetType::Unknown;

	// Prepare transaction files for the asset to be deleted.
	if (!Prepare(assetManager, guid, pair, type, error))
	{
		return false;
	}

	// Reject deletion of ActorImprint assetManager, which require a reference-aware workflow.
	if (type == AssetType::ActorImprint)
	{
		error = "ActorImprint deletion requires its reference-aware workflow.";
		return false;
	}

	// Move the asset data and meta file to temporary paths for deletion.
	const std::string relative = pair.data[0].lexically_relative(assetManager.GetAssetRoot()).generic_string();

	if (!pair.MoveTo(1))
	{
		return Reject(pair, error, "Could not stage the asset for deletion.");
	}

	// Notify the AssetManager of the asset change to update the catalog.
	AssetCatalogError catalogError;

	if (!assetManager.NotifyAssetChanged(relative, &catalogError))
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
