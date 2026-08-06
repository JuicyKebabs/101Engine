#include "ComponentSerializer.h"
#include "Engine/Component/Component.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/Debug/Debug.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

bool ComponentSerializer::SerializeRecord(const Component* component, nlohmann::json& outJson)
{
	if (!component || component->IsDestroyed()) return false;

	// Get name of the component type from the ComponentRegistry
	const std::type_index typeId = typeid(*component);
	const std::string typeName = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

	if (typeName.empty())
	{
		DBG("ComponentSerializer::SerializeRecord: Component type '%s' is not registered.", typeId.name());
		return false;
	}

	// Serialize the component's data into a JSON object
	json componentData;
	if (!component->Serialize(componentData))
	{
		DBG("ComponentSerializer::SerializeRecord: Failed to serialize component '%s'.", typeName.c_str());
		return false;
	}

	// Create a record for the component with its type and serialized data
	json componentRecord;
	componentRecord["type"] = typeName;
	componentRecord["data"] = std::move(componentData);

	outJson = std::move(componentRecord);

	return true;
}
