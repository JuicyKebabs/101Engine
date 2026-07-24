#include "Canvas.h"
#include <limits>

bool Canvas::Serialize(
	nlohmann::json& outJson) const
{
	if (!Component::Serialize(outJson)) return false;

	outJson["sortOrder"] = m_sortOrder;
	outJson["visible"] = m_isVisible;

	return true;
}

bool Canvas::Deserialize(
	const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	// Validate the required fields in the JSON object
	if (!json.contains("name")		||
		!json.contains("sortOrder") ||
		!json.contains("visible"))
	{
		return false;
	}

	// Validate the types of the fields
	if (!json["name"].is_string()				||
		!json["sortOrder"].is_number_integer()	||
		!json["visible"].is_boolean())
	{
		return false;
	}

	std::string parsedName;
	int64_t parsedSortOrder = 0;
	bool parsedVisible = true;

	// Attempt to parse the values from the JSON object
	try
	{
		parsedName = json["name"].get<std::string>();
		parsedSortOrder = json["sortOrder"].get<int64_t>();
		parsedVisible = json["visible"].get<bool>();
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}

	// Validate the parsed sort order to ensure it is within the valid range for UINT
	if (parsedSortOrder < 0 ||
		static_cast<uint64_t>(parsedSortOrder) >
		static_cast<uint64_t>((std::numeric_limits<UINT>::max)()))
	{
		return false;
	}

	// Apply the parsed values to the Canvas component's parameters
	ParamDesc parsedDesc;
	parsedDesc.name = parsedName;
	parsedDesc.sortOrder =
		static_cast<UINT>(parsedSortOrder);
	parsedDesc.isVisible = parsedVisible;

	SetParams(parsedDesc);

	return true;
}