#pragma once

#include "Engine/Core/GUID/Guid.h"

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
	static bool Create(std::string_view name, SceneBase& scene, AssetManager& assets,
		Guid& outGuid, std::string& outPath, SceneAssetWorkflowError* outError = nullptr);
	static bool Rename(const Guid& guid, std::string_view newName, AssetManager& assets,
		std::string& outPath, SceneAssetWorkflowError* outError = nullptr);
	static bool IsManagedAssetPath(std::string_view relativePath);

private:
	static bool NormalizeName(std::string_view input, std::string& outName,
		SceneAssetWorkflowError* outError);
};
