#include "AddComponentCommand.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Component.h"
#include "Engine/Scene/ComponentRegistry.h"

AddComponentCommand::AddComponentCommand(
	SceneBase* scene,
	const Guid& actorGuid,
	const std::string& componentName
)
	: m_scene(scene)
	, m_actorGuid(actorGuid)
	, m_componentName(componentName)
{}

bool AddComponentCommand::Execute()
{
	// Apply only once
	if (m_isApplied) return false;

	if (m_hasExecuted)
	{// In case of Redo
		Component* restored = m_componentSnapshot.Restore(m_scene);

		if (!restored) return false;

		m_isApplied = true;
		return true;
	}

	// Resolve actor from scene by its GUID for the first execution
	Actor* actor = ResolveActor();
	if (!actor) return false;

	auto& registry = ComponentRegistry::Get();
	const auto typeId = registry.GetTypeId(m_componentName);

	if (!typeId) return false;

	std::unique_ptr<Component> component = nullptr;

	// Check if it's allowed to add given component type
	if (!registry.CanAddToActor(m_componentName, actor)) return false;

	// Set the occurrence index of the component to be added
	// at the end of the vector which stores same type components in the actor
	m_occurrenceIndex = actor->GetComponentsByExactType(*typeId).size();

	// Create a new component instance from the registry
	component.reset(registry.Create(m_componentName));

	if (!component) return false;

	Component* added = m_scene->AddActorComponentImmediate(actor, std::move(component), m_occurrenceIndex);

	if (!added) return false;

	m_hasExecuted = true;
	m_isApplied = true;

	return true;
}

bool AddComponentCommand::Undo()
{
	// Undo only if the command has been executed and applied
	if (!m_hasExecuted || !m_isApplied) return false;

	// Resolve actor from scene by its GUID
	Actor* actor = ResolveActor();
	if (!actor) return false;

	// Resolve component from actor by its name
	Component* component = ResolveComponent(actor);
	if (!component) return false;

	// Capture the component state before removing it
	if (!m_componentSnapshot.Capture(actor, component)) return false;

	// Remove the component from the actor immediately
	if (!m_scene->RemoveActorComponentImmediate(actor, component)) return false;

	m_isApplied = false;
	return true;
}

Actor* AddComponentCommand::ResolveActor() const
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

Component* AddComponentCommand::ResolveComponent(Actor* actor) const
{
	if (!actor) return nullptr;

	const auto typeId = ComponentRegistry::Get().GetTypeId(m_componentName);

	if (!typeId) return nullptr;

	return actor->GetComponentByExactType(
		*typeId,
		m_occurrenceIndex
	);
}

