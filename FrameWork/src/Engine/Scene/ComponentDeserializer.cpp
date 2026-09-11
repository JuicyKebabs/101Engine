#include "ComponentDeserializer.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/ComponentReflection.h"
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

	const bool hasType = componentJson.contains("type");
	const bool hasData = componentJson.contains("data");
	const bool hasValidType = hasType && componentJson["type"].is_string();
	const bool hasValidData = hasData && componentJson["data"].is_object();
	if (!hasValidType || !hasValidData)
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component record must contain a string 'type' and an object 'data'.");
		return nullptr;
	}

	// Get the component type name and data from the JSON record
	const std::string componentTypeName = componentJson["type"].get<std::string>();
	const json& componentData = componentJson["data"];

	if (componentTypeName.empty())
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component type name is empty.");
		return nullptr;
	}

	// Create the component instance with the registered factory function for the given type name
	std::unique_ptr<Component> component(ComponentRegistry::Get().Create(componentTypeName));

	if (!component)
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component '%s' is not registered.", componentTypeName.c_str());
		return nullptr;
	}

	// Deserialize the component data
	ReflectionError error;
	if (!DeserializeReflectedComponent(*component, componentData, &error))
	{
		std::string path = "<type>";
		if (error.path)
		{
			path = error.path->ToString();
		}
		DBG(
			"ComponentDeserializer::DeserializeRecord: Failed to deserialize component '%s' at '%s': %s",
			componentTypeName.c_str(), path.c_str(), error.message.c_str());
		return nullptr;
	}

	return component;
}
