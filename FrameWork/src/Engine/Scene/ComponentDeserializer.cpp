#include "ComponentDeserializer.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/ComponentReflection.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/Debug/Debug.h"

using json = nlohmann::json;

std::unique_ptr<Component> ComponentDeserializer::DeserializeRecord(
	const json& componentJson,
	ComponentDeserializationError* outError)
{
	if (outError) *outError = {};
	const auto Fail = [&](std::string path, std::string message) -> std::unique_ptr<Component>
	{
		if (outError) *outError = { std::move(path), std::move(message) };
		return nullptr;
	};

	// Validate the component record structure
	if (!componentJson.is_object())
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component record must be an object.");
		return Fail({}, "Component record must be an object.");
	}

	const bool hasType = componentJson.contains("type");
	const bool hasData = componentJson.contains("data");
	const bool hasValidType = hasType && componentJson["type"].is_string();
	const bool hasValidData = hasData && componentJson["data"].is_object();
	if (!hasValidType || !hasValidData)
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component record must contain a string 'type' and an object 'data'.");
		if (!hasValidType) return Fail("/type", "Component type must be a string.");
		return Fail("/data", "Component data must be an object.");
	}

	// Get the component type name and data from the JSON record
	const std::string componentTypeName = componentJson["type"].get<std::string>();
	const json& componentData = componentJson["data"];

	if (componentTypeName.empty())
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component type name is empty.");
		return Fail("/type", "Component type name must not be empty.");
	}

	// Create the component instance with the registered factory function for the given type name
	std::unique_ptr<Component> component(ComponentRegistry::Get().Create(componentTypeName));

	if (!component)
	{
		DBG("ComponentDeserializer::DeserializeRecord: Component '%s' is not registered.", componentTypeName.c_str());
		return Fail("/type", "Component type is not registered: " + componentTypeName);
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
		return Fail("/data" + (error.path ? error.path->ToString() : std::string{}), error.message);
	}

	return component;
}
