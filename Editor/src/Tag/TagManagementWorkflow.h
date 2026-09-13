#pragma once

#include "Engine/Actor/ActorTag.h"
#include <string>
#include <string_view>
#include <vector>

class AssetManager;
class EditorDocumentManager;
class ProjectSettings;

struct TagUsage
{
	std::string location;
};

struct TagManagementResult
{
	std::string error;
	std::vector<TagUsage> usages;
	explicit operator bool() const { return error.empty() && usages.empty(); }
};

class TagManagementWorkflow
{
public:
	static TagManagementResult Create(std::string_view name, ProjectSettings& settings,
		const std::string& settingsPath);
	static TagManagementResult Rename(std::string_view oldName, std::string_view newName,
		ProjectSettings& settings, const std::string& settingsPath,
		const AssetManager& assets, const EditorDocumentManager& documents);
	static TagManagementResult Delete(std::string_view name, ProjectSettings& settings,
		const std::string& settingsPath, const AssetManager& assets,
		const EditorDocumentManager& documents);

private:
	static TagManagementResult FindUsages(std::string_view name,
		const AssetManager& assets, const EditorDocumentManager& documents);
};
