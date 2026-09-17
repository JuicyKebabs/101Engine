#include "MetaFile.h"
#include "nlohmann/json.hpp"
#include <Windows.h>
#include <fstream>
#include <filesystem>

std::optional<Guid> MetaFile::TryLoad(const std::string& path)
{
	try
	{
		std::ifstream file(path + ".meta");

		if (!file)
		{
			return std::nullopt;
		}

		bool duplicateGuid = false;
		bool sawGuid = false;
		auto callback = [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& value)
		{
			if (depth == 1 && event == nlohmann::json::parse_event_t::key && value == "guid")
			{
				duplicateGuid = sawGuid;
				sawGuid = true;
			}

			return true;
		};
		const auto json = nlohmann::json::parse(file, callback);

		if (file.bad() || duplicateGuid || !json.is_object() || !json.contains("guid") || !json["guid"].is_string())
		{
			return std::nullopt;
		}

		const auto text = json["guid"].get<std::string>();
		Guid guid;

		if (text.find('\0') != std::string::npos || !Guid::TryParse(text, guid))
		{
			return std::nullopt;
		}

		return guid;
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}

bool MetaFile::Save(const std::string& path, const Guid& guid, WriteMode mode)
{
	if (!guid.IsValid())
	{
		return false;
	}

	std::string metaPath = path + ".meta";

	// Create a JSON object and set the "guid" field to the string representation of the provided Guid
	nlohmann::json j;
	j["guid"] = guid.ToString();

	const auto contents = j.dump(4);
	const HANDLE file = CreateFileW(std::filesystem::path(metaPath).c_str(), GENERIC_WRITE, 0, nullptr,
		mode == WriteMode::CreateNew ? CREATE_NEW : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD written = 0;
	const bool success = WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) &&
		written == contents.size() && FlushFileBuffers(file);
	const bool closed = CloseHandle(file) != FALSE;
	return success && closed;
}
