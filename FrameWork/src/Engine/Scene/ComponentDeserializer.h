#pragma once
#include <memory>
#include <string>
#include "nlohmann/json.hpp"

//--------------------------------------------------------------------------------
// ComponentDeserializer class
// This class provides functionality to deserialize a component from JSON format.
// Just creating a component from JSON does not attach it to an actor.
//--------------------------------------------------------------------------------

class Component;

struct ComponentDeserializationError
{
	std::string path;
	std::string message;
};

class ComponentDeserializer
{
public:
	static std::unique_ptr<Component> DeserializeRecord(
		const nlohmann::json& componentJson,
		ComponentDeserializationError* outError = nullptr);
};
