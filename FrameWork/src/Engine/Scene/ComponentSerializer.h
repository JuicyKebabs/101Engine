#pragma once
#include "nlohmann/json.hpp"

class Component;

//---------------------------------------------------------------------------------------------
// ComponentSerializer class
// This class provides functionality to serialize a component into JSON format.
// Just creating a JSON record does not perform file I/O, it only prepares the data for saving.
//---------------------------------------------------------------------------------------------

class ComponentSerializer
{
public:
	static bool SerializeRecord(const Component* component, nlohmann::json& outJson);
};