#pragma once

#include "Engine/Core/GUID/Guid.h"

#include <filesystem>
#include <string>
#include <string_view>

class AssetManager;
class SceneBase;

struct SceneAssetWorkflowError
{
	std::string path;
	std::string message;
};

class SceneAssetWorkflow
{
public:
	// Entrance point for creating a new Scene asset.
	// Creates the Scene file and its metadata sidecar, then refreshes the AssetManager's catalog.
	static bool Create(
		const std::filesystem::path& relativeDirectory,	// Where to create the new scene. (relative path from the AssetManager's root)
		std::string_view name,							// The new scene's name (without extension).
		SceneBase& scene,
		AssetManager& assetManager,
		Guid& outGuid,									// Created Scene's AssetGUID (output).
		std::string& outPath,							// Created Scene's absolute path (output).
		SceneAssetWorkflowError* outError = nullptr);

	static bool Rename(
		const Guid& guid,			// The AssetGUID of the Scene to rename.
		std::string_view newName,	// The new name for the Scene (without extension).
		AssetManager& assetsassetManager,
		std::string& outPath,		// Renamed Scene's absolute path (output).
		SceneAssetWorkflowError* outError = nullptr);

private:
	static bool NormalizeName(
		std::string_view input,
		std::string& outName,
		SceneAssetWorkflowError* outError);
};
