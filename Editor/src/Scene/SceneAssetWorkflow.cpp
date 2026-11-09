#include "SceneAssetWorkflow.h"
#include "Core/AssetFileWorkflow.h"
#include "Core/AssetCreationPath.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Scene/SceneWriter.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace
{

bool Fail(SceneAssetWorkflowError* error, const fs::path& path, std::string message)
{
	if (error)
	{
		*error = {path.generic_string(), std::move(message)};
	}

	return false;
}

bool Move(const fs::path& from, const fs::path& to)
{
	return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
}

}

bool SceneAssetWorkflow::NormalizeName(
	std::string_view input,
	std::string& outName,
	SceneAssetWorkflowError* outError)
{
	outName.assign(input);
	while (!outName.empty() && std::isspace(static_cast<unsigned char>(outName.front())))
		outName.erase(outName.begin());
	while (!outName.empty() && std::isspace(static_cast<unsigned char>(outName.back())))
		outName.pop_back();

	if (outName.size() > 6)
	{
		std::string suffix = outName.substr(outName.size() - 6);
		std::transform(
			suffix.begin(),
			suffix.end(),
			suffix.begin(),
			[](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		if (suffix == ".scene")
		{
			outName.resize(outName.size() - 6);
		}
	}

	if (outName.empty() || outName == "." || outName == "..")
	{
		return Fail(outError, {}, "Scene name must not be empty.");
	}

	if (outName.find_first_of("<>:\"/\\|?*") != std::string::npos ||
		outName.back() == '.' || outName.back() == ' ')
	{
		return Fail(outError, {}, "Scene name contains characters that are not valid in a Windows filename.");
	}

	std::string upper = outName;
	std::transform(
		upper.begin(),
		upper.end(),
		upper.begin(),
		[](unsigned char c)
		{
			return static_cast<char>(std::toupper(c));
		});

	static constexpr const char* reserved[] = {
		"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

	for (const char* value : reserved)
	{
		if (upper == value)
		{
			return Fail(outError, {}, "Scene name is reserved by Windows.");
		}
	}

	return true;
}

bool SceneAssetWorkflow::Create(
	const fs::path& relativeDirectory,
	std::string_view name,
	SceneBase& scene,
	AssetManager& assetManager,
	Guid& outGuid,
	std::string& outPath,
	SceneAssetWorkflowError* outError)
{
	if (outError)
	{
		*outError = {};
	}

	outGuid = {};
	outPath.clear();
	std::string normalizedName;

	if (!NormalizeName(name, normalizedName, outError))
	{
		return false;
	}

	fs::path directory;
	std::string pathError;

	if (!AssetCreationPath::ResolveDirectory(assetManager, relativeDirectory, directory, pathError))
	{
		return Fail(outError, relativeDirectory, pathError);
	}

	const fs::path destination = directory / fs::u8path(normalizedName + ".scene");

	if (!AssetCreationPath::CheckAvailable(directory, destination.filename(), pathError))
	{
		return Fail(outError, destination, pathError);
	}

	std::error_code error;

	if (!SceneWriter::SaveScene(destination.string(), &scene))
	{
		return Fail(outError, destination, "Could not save the initial Scene.");
	}

	AssetCatalogError catalogError;

	if (!assetManager.Refresh(&catalogError))
	{
		fs::remove(destination, error);
		fs::remove(destination.string() + ".meta", error);
		assetManager.Refresh();
		return Fail(outError, catalogError.path, catalogError.message);
	}

	const std::string relative = destination.lexically_relative(assetManager.GetAssetRoot()).generic_string();
	const AssetEntry* entry = assetManager.GetAssetEntryByPath(relative);

	if (!entry || entry->type != AssetType::Scene)
	{
		fs::remove(destination, error);
		fs::remove(destination.string() + ".meta", error);
		assetManager.Refresh();
		return Fail(outError, destination, "The created Scene was not published by the Asset Catalog.");
	}

	outGuid = entry->guid;
	outPath = destination.string();
	return true;
}

bool SceneAssetWorkflow::Rename(
	const Guid& guid,
	std::string_view newName,
	AssetManager & assets,
	std::string & outPath,
	SceneAssetWorkflowError * outError)
{
	outPath.clear();

	if (outError)
	{
		outError->path.clear();
		outError->message.clear();
	}

	const AssetEntry* entry = assets.GetAssetEntry(guid);
	std::filesystem::path errorPath;
	
	if (!entry || entry->type != AssetType::Scene)
	{
		return Fail(outError, errorPath, "Scene asset is unavailable.");
	}
	
	errorPath = std::filesystem::path(entry->relativePath);
	std::string error;
	
	if (!AssetFileWorkflow::Rename(assets, guid, newName, outPath, error))
	{
		return Fail(outError, errorPath, error);
	}

	return true;
}
