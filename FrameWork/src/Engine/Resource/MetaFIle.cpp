#include "MetaFile.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <filesystem>

std::optional<Guid> MetaFile::TryLoad(const std::string& path)
{
	// Check if the .meta file exists
	std::string metaPath = path + ".meta";
	if (!std::filesystem::exists(metaPath)) return std::nullopt;

	// Open the .meta file and read its contents
	std::ifstream f(metaPath);
	nlohmann::json j;
	f >> j;

	// Check if the field "guid" exists in the JSON
	if (!j.contains("guid")) return std::nullopt;

	// Return the Guid by converting the string representation to a Guid object
	return Guid::FromString(j["guid"].get<std::string>());
}

void MetaFile::Save(const std::string& path, const Guid& guid)
{
	std::string metaPath = path + ".meta";

	// Create a JSON object and set the "guid" field to the string representation of the provided Guid
	nlohmann::json j;
	j["guid"] = guid.ToString();

	// Open the .meta file and write the JSON object to it with an indentation of 4 spaces
	std::ofstream f(metaPath);
	f << j.dump(4);
}