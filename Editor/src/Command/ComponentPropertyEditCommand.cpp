#include "ComponentPropertyEditCommand.h"

#include "Engine/Actor/Actor.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"

ComponentPropertyEditCommand::ComponentPropertyEditCommand(
	SceneBase* scene,
	ComponentPropertyIdentity identity,
	PropertyValue before,
	PropertyValue after)
	: m_scene(scene),
	  m_identity(std::move(identity)),
	  m_before(std::move(before)),
	  m_after(std::move(after))
{}

bool ComponentPropertyEditCommand::Execute()
{
	return ApplyComponentPropertyValue(m_scene, m_identity, m_after);
}

bool ComponentPropertyEditCommand::Undo()
{
	return ApplyComponentPropertyValue(m_scene, m_identity, m_before);
}

bool ApplyComponentPropertyValue(
	SceneBase* scene,
	const ComponentPropertyIdentity& identity,
	const PropertyValue& value)
{
	if (!scene || !identity.actorGuid.IsValid()) return false;

	Actor* actor = scene->ResolveActor(identity.actorGuid);
	if (!actor) return false;
	if (actor->IsDestroyed()) return false;
	if (actor->GetOwner() != scene) return false;

	Component* component = actor->GetComponentByExactType(
		identity.componentType, identity.occurrenceIndex);
	if (!component) return false;

	const TypeMetadata* metadata = ComponentRegistry::Get().GetMetadata(identity.componentType);
	if (!metadata) return false;

	const PropertyMetadata* property = metadata->FindPropertyByPath(identity.propertyPath);
	if (!property) return false;

	return metadata->TryWriteProperty(
		identity.componentType, component, *property, value);
}
