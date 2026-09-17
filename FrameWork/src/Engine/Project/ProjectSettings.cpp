#include "ProjectSettings.h"

#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Actor/ActorTag.h"
#include "nlohmann/json.hpp"

#include <Windows.h>
#include <fstream>
#include <algorithm>

namespace
{
	bool Fail(std::string* error, std::string message)
	{
		if (error)
		{
			*error = std::move(message);
		}

		return false;
	}

	bool ReadGuid(
		const nlohmann::json& root,
		const char* section,
		Guid& outGuid,
		std::string* error)
	{
		if (!root.contains(section))
		{
			return true;
		}

		const auto& object = root[section];

		if (!object.is_object())
		{
			return Fail(error, std::string(section) + " must be an object.");
		}

		if (!object.contains("startupSceneGuid") || object["startupSceneGuid"].is_null())
		{
			return true;
		}

		if (!object["startupSceneGuid"].is_string())
		{
			return Fail(error, std::string(section) + ".startupSceneGuid must be a GUID string.");
		}

		Guid parsed;

		if (!Guid::TryParse(object["startupSceneGuid"].get<std::string>(), parsed))
		{
			return Fail(error, std::string(section) + ".startupSceneGuid is invalid.");
		}

		outGuid = parsed;
		return true;
	}

	void WriteGuid(nlohmann::json& root, const char* section, const Guid& guid)
	{
		if (!root.contains(section) || !root[section].is_object())
		{
			root[section] = nlohmann::json::object();
		}

		if (guid.IsValid())
		{
			root[section]["startupSceneGuid"] = guid.ToString();
		}
		else
		{
			root[section].erase("startupSceneGuid");
		}
	}

	bool ReadTags(const nlohmann::json& root, std::vector<std::string>& outTags, std::string* error)
	{
		if (!root.contains("tags"))
		{
			return true;
		}

		if (!root["tags"].is_array())
		{
			return Fail(error, "tags must be an array of strings.");
		}

		std::vector<std::string> input;

		for (const auto& value : root["tags"])
		{
			if (!value.is_string())
			{
				return Fail(error, "tags must contain only strings.");
			}

			input.push_back(value.get<std::string>());
		}

		return TagRegistry::ValidateUserTagSet(input, outTags, error);
	}
}

bool ProjectSettings::Load(
	const std::filesystem::path& path,
	ProjectSettings& outSettings,
	std::string* outError)
{
	if (outError)
	{
		outError->clear();
	}

	try
	{
		std::ifstream stream(path);

		if (!stream)
		{
			return Fail(outError, "Could not open project settings.");
		}

		const nlohmann::json root = nlohmann::json::parse(stream);

		if (!root.is_object())
		{
			return Fail(outError, "Project settings root must be an object.");
		}

		ProjectSettings candidate;

		if (!ReadGuid(root, "editor", candidate.m_editorStartupSceneGuid, outError) ||
			!ReadGuid(root, "game", candidate.m_gameStartupSceneGuid, outError) ||
			!ReadTags(root, candidate.m_userTags, outError))
		{
			return false;
		}

		outSettings = candidate;
		return true;
	}
	catch (const std::exception& exception)
	{
		return Fail(outError, exception.what());
	}
}

bool ProjectSettings::Save(const std::filesystem::path& path, std::string* outError) const
{
	if (outError)
	{
		outError->clear();
	}

	try
	{
		nlohmann::json root = nlohmann::json::object();
		{
			std::ifstream input(path);

			if (input)
			{
				root = nlohmann::json::parse(input);

				if (!root.is_object())
				{
					return Fail(outError, "Project settings root must be an object.");
				}
			}
		}

		if (!root.contains("version"))
		{
			root["version"] = 1;
		}

		WriteGuid(root, "editor", m_editorStartupSceneGuid);
		WriteGuid(root, "game", m_gameStartupSceneGuid);
		std::vector<std::string> tags;

		if (!TagRegistry::ValidateUserTagSet(m_userTags, tags, outError))
		{
			return false;
		}

		root["tags"] = tags;

		const std::filesystem::path temporary = path.parent_path() /
			(path.filename().wstring() + L"." +
			 std::filesystem::path(GuidGenerator::Generate().ToString()).wstring() + L".tmp");
		{
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);

			if (!output)
			{
				return Fail(outError, "Could not create temporary project settings.");
			}

			output << root.dump(2);
			output.flush();

			if (!output)
			{
				std::filesystem::remove(temporary);
				return Fail(outError, "Could not write project settings.");
			}
		}

		if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			const DWORD code = GetLastError();
			std::filesystem::remove(temporary);
			return Fail(outError, "Could not replace project settings (Windows error " + std::to_string(code) + ").");
		}

		return true;
	}
	catch (const std::exception& exception)
	{
		return Fail(outError, exception.what());
	}
}
