#include "SceneAssetWorkflow.h"
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

bool ExistsCaseInsensitive(
	const fs::path& directory,
	const fs::path& filename,
	const fs::path& ignored = {})
{
	std::error_code error;

	for (const auto& item : fs::directory_iterator(directory, error))
	{
		if (error)
		{
			return true;
		}

		if (!ignored.empty() && fs::equivalent(item.path(), ignored, error))
		{
			error.clear();
			continue;
		}

		auto left = item.path().filename().wstring();
		auto right = filename.wstring();

		if (_wcsicmp(left.c_str(), right.c_str()) == 0)
		{
			return true;
		}
	}

	return error.value() != 0;
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
	AssetManager& assetManager,
	std::string& outPath,
	SceneAssetWorkflowError* outError)
{
	if (outError)
	{
		*outError = {};
	}

	outPath.clear();
	const AssetEntry* entry = assetManager.GetAssetEntry(guid);

	if (!entry || entry->type != AssetType::Scene)
	{
		return Fail(outError, {}, "Scene asset is unavailable.");
	}

	std::string normalizedName;

	if (!NormalizeName(newName, normalizedName, outError))
	{
		return false;
	}

	const fs::path relativeAsset(entry->relativePath);
	fs::path relativeDirectory = relativeAsset.parent_path();

	if (relativeDirectory.empty())
	{
		relativeDirectory = ".";
	}

	fs::path directory;
	std::string pathError;

	if (!AssetCreationPath::ResolveDirectory(assetManager, relativeDirectory, directory, pathError))
	{
		return Fail(outError, relativeDirectory, pathError);
	}

	const fs::path source = directory / relativeAsset.filename();

	const fs::path sourceMeta = source.string() + ".meta";
	const fs::path destination = source.parent_path() / (normalizedName + ".scene");
	const fs::path destinationMeta = destination.string() + ".meta";

	if (source.filename() == destination.filename())
	{
		outPath = source.string();
		return true;
	}

	if (ExistsCaseInsensitive(source.parent_path(), destination.filename(), source))
	{
		return Fail(outError, destination, "A Scene with this name already exists.");
	}

	if (!fs::exists(sourceMeta))
	{
		return Fail(outError, sourceMeta, "Scene metadata is missing.");
	}

	const fs::path staging =
		source.parent_path() /
		(source.filename().wstring() + L"." + fs::path(GuidGenerator::Generate().ToString()).wstring() + L".rename");
	
	const fs::path stagingMeta = staging.string() + ".meta";

	if (!Move(source, staging))
	{
		return Fail(outError, source, "Could not stage the Scene for rename.");
	}

	if (!Move(sourceMeta, stagingMeta))
	{
		Move(staging, source);
		return Fail(outError, sourceMeta, "Could not stage Scene metadata for rename.");
	}

	if (!Move(staging, destination) || !Move(stagingMeta, destinationMeta))
	{
		if (fs::exists(destination))
		{
			Move(destination, staging);
		}

		if (fs::exists(destinationMeta))
		{
			Move(destinationMeta, stagingMeta);
		}

		Move(staging, source);
		Move(stagingMeta, sourceMeta);
		return Fail(outError, destination, "Could not publish the renamed Scene; the original was restored.");
	}

	AssetCatalogError catalogError;

	if (!assetManager.Refresh(&catalogError))
	{
		Move(destination, staging);
		Move(destinationMeta, stagingMeta);
		Move(staging, source);
		Move(stagingMeta, sourceMeta);

		assetManager.Refresh();

		return Fail(outError, catalogError.path, catalogError.message);
	}

	const AssetEntry* renamed = assetManager.GetAssetEntry(guid);

	if (!renamed || renamed->type != AssetType::Scene)
	{
		Move(destination, staging);
		Move(destinationMeta, stagingMeta);
		Move(staging, source);
		Move(stagingMeta, sourceMeta);

		assetManager.Refresh();

		return Fail(outError, destination, "The Asset Catalog lost the Scene identity after rename.");
	}

	outPath = assetManager.GetAssetPath(guid);
	return true;
}
