#pragma once

#include "Engine/Core/GUID/Guid.h"

#include <filesystem>
#include <string>
#include <vector>
#include <utility>

class ProjectSettings
{
public:
	static bool Load(
		const std::filesystem::path& path,
		ProjectSettings& outSettings,
		std::string* outError = nullptr);
	bool Save(const std::filesystem::path& path, std::string* outError = nullptr) const;

	const Guid& GetEditorStartupSceneGuid() const { return m_editorStartupSceneGuid; }
	const Guid& GetGameStartupSceneGuid() const { return m_gameStartupSceneGuid; }
	void SetEditorStartupSceneGuid(const Guid& guid) { m_editorStartupSceneGuid = guid; }
	void SetGameStartupSceneGuid(const Guid& guid) { m_gameStartupSceneGuid = guid; }
	const std::vector<std::string>& GetUserTags() const { return m_userTags; }
	void SetUserTags(std::vector<std::string> tags) { m_userTags = std::move(tags); }

private:
	Guid m_editorStartupSceneGuid;
	Guid m_gameStartupSceneGuid;
	std::vector<std::string> m_userTags;
};
