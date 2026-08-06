#include "ComponentSnapshot.h"
#include "Engine/Component/Component.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Scene/ComponentDeserializer.h"
#include "Engine/Scene/ComponentSerializer.h"
#include "Engine/Scene/SceneBase.h"

#include <algorithm>
#include <iterator>
#include <typeindex>
#include <vector>

using json = nlohmann::json;

bool ComponentSnapshot::Capture(Actor* actor, Component* component)
{
	m_actorGuid = {};
	m_componentRecord = {};
	m_occurrenceIndex = 0;
	m_isValid = false;

	if (!actor ||
		!component ||
		!actor->GetGuid().IsValid() ||
		component->GetOwner() != actor ||
		component->IsDestroyed())
	{
		return false;
	}

	// Get the type index of the component and all components of the same type from the actor
	const std::type_index typeId(typeid(*component));
	const std::vector<Component*> components = actor->GetComponentsByExactType(typeId);

	// Check if the given componet is owned by the given actor
	auto componentIt = std::find(components.begin(), components.end(), component);

	if (componentIt == components.end())
	{
		return false;
	}

	json componentRecord;

	// Serialize the component into a JSON record
	if (!ComponentSerializer::SerializeRecord(component, componentRecord))
	{
		return false;
	}

	// Store occurrence index
	m_occurrenceIndex = static_cast<std::size_t>(std::distance(components.begin(), componentIt));

	// Store the identity of the Actor that owns the captured Component
	m_actorGuid = actor->GetGuid();

	// Store the serialized component record
	m_componentRecord = std::move(componentRecord);

	m_isValid = true;

	return true;
}

Component* ComponentSnapshot::Restore(SceneBase* scene) const
{
	if (!scene ||
		!m_isValid ||
		!m_actorGuid.IsValid())
	{
		return nullptr;
	}

	// Resolve the originating Actor by its persistent identity.
	Actor* actor = scene->ResolveActor(m_actorGuid);

	if (!actor ||
		actor->IsDestroyed() ||
		actor->GetOwner() != scene)
	{
		return nullptr;
	}

	std::unique_ptr<Component> component =
		ComponentDeserializer::DeserializeRecord(m_componentRecord);

	if (!component) return nullptr;

	// Restore the Component to the exact-type occurrence position captured in the Memento.
	Component* restored = scene->AddActorComponentImmediate(
		actor,
		std::move(component),
		m_occurrenceIndex
	);

	if (!restored) return nullptr;

	// Reference resolution requires the restored Component to already belong to an Actor and Scene.
	if (!restored->ResolveReferences(*scene))
	{
		if (!scene->RemoveActorComponentImmediate(actor, restored))
		{
			DBG("ComponentSnapshot::Restore: Failed to roll back a Component after reference resolution failed.");
		}

		return nullptr;
	}

	return restored;
}
