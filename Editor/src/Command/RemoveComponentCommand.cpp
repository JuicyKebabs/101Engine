#include "RemoveComponentCommand.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Component.h"
#include "Engine/Scene/ComponentRegistry.h"

RemoveComponentCommand::RemoveComponentCommand(
	SceneBase* scene,
	const Guid& actorGuid,
	const std::string& componentName,
	std::size_t occurrenceIndex
)
	: m_scene(scene)
	, m_actorGuid(actorGuid)
	, m_componentName(componentName)
	, m_occurrenceIndex(occurrenceIndex)
{}

bool RemoveComponentCommand::Execute()
{
	// Apply only when the component is not already removed
	if (m_isRemoved) return false;

	// Resolve actor from scene by its GUID
	Actor* actor = ResolveActor();
	if (!actor) return false;

	// Resolve component from actor by its occurrence index
	Component* component = ResolveComponent(actor);
	if (!component) return false;

	if (!m_hasSnapshot)
	{// First execution

		// Capture the component snapshot before removal
		if (!m_componentSnapshot.Capture(actor, component))
		{
			return false;
		}
		
		m_hasSnapshot = true;
	}

	// Remove the component from the actor immediately
	if (!m_scene->RemoveActorComponentImmediate(actor, component))
	{
		return false;
	}

	m_isRemoved = true;
	return true;
}

bool RemoveComponentCommand::Undo()
{
	if (!m_isRemoved ||
		!m_hasSnapshot ||
		!m_componentSnapshot.IsValid())
	{
		return false;
	}

	// Restore the component from the snapshot
	Component* restored = m_componentSnapshot.Restore(m_scene);
	if (!restored) return false;

	m_isRemoved = false;	// Mark as not removed to allow re-execution
	return true;
}

Actor* RemoveComponentCommand::ResolveActor() const
{
	if (!m_scene || !m_actorGuid.IsValid()) return nullptr;

	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	if (!actor ||
		actor->IsDestroyed() ||
		actor->GetOwner() != m_scene)
	{
		return nullptr;
	}

	return actor;
}

Component* RemoveComponentCommand::ResolveComponent(Actor* actor) const
{
	if (!actor) return nullptr;

	const auto typeId = ComponentRegistry::Get().GetTypeId(m_componentName);

	if (!typeId) return nullptr;

	return actor->GetComponentByExactType(
		*typeId,
		m_occurrenceIndex
	);
}

