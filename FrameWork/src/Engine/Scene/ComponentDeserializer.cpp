#include "ComponentDeserializer.h"
#include "Engine/Component/Component.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/Debug/Debug.h"

using json = nlohmann::json;

std::unique_ptr<Component> ComponentDeserializer::DeserializeRecord(const json& componentJson)
{
	// Validate the component record structure
	if (!componentJson.is_object())
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component record must be an object.");
		return nullptr;
	}

	if (!componentJson.contains("type") ||
		!componentJson["type"].is_string() ||
		!componentJson.contains("data") ||
		!componentJson["data"].is_object())
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component record must contain a string 'type' and an object 'data'.");
		return nullptr;
	}

	// get the component type name and data from the JSON record
	const std::string componentTypeName = componentJson["type"].get<std::string>();
	const json& componentData = componentJson["data"];

	if (componentTypeName.empty())
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component type name is empty.");
		return nullptr;
	}

	// Create the component instance
	std::unique_ptr<Component> component(ComponentRegistry::Get().Create(componentTypeName));

	if (!component)
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component '%s' is not registered.", componentTypeName.c_str());
		return nullptr;
	}

	// Deserialize the component data
	if (!component->Deserialize(componentData))
	{
		DBG("ComponentDeserializer::DeserializeRecord: Failed to deserialize component '%s'.", componentTypeName.c_str());
		return nullptr;
	}

	return component;
}
