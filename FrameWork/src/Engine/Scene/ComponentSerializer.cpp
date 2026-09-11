#include "ComponentSerializer.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/ComponentReflection.h"
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
	const Actor* owner = component->GetOwner();
	const SceneBase* scene = nullptr;
	if (owner)
	{
		scene = owner->GetOwner();
	}
	ReflectionError error;
	if (!SerializeReflectedComponent(*component, componentData, scene, &error))
	{
		std::string path = "<type>";
		if (error.path)
		{
			path = error.path->ToString();
		}
		DBG(
			"ComponentSerializer::SerializeRecord: Failed to serialize component '%s' at '%s': %s",
			typeName.c_str(), path.c_str(), error.message.c_str());
		return false;
	}

	// Create a record for the component with its type and serialized data
	json componentRecord;
	componentRecord["type"] = typeName;
	componentRecord["data"] = std::move(componentData);

	outJson = std::move(componentRecord);

	return true;
}
