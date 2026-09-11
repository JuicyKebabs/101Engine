#pragma once

#include "Command/IEditorCommand.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/GUID/Guid.h"
#include <cstddef>
#include <typeindex>

class SceneBase;

struct ComponentPropertyIdentity
{
	Guid actorGuid;
	std::type_index componentType;
	std::size_t occurrenceIndex;
	PropertyPath propertyPath;
};

bool ApplyComponentPropertyValue(
	SceneBase* scene,
	const ComponentPropertyIdentity& identity,
	const PropertyValue& value);

class ComponentPropertyEditCommand : public IEditorCommand
{
public:
	ComponentPropertyEditCommand(
		SceneBase* scene,
		ComponentPropertyIdentity identity,
		PropertyValue before,
		PropertyValue after);

	bool Execute() override;
	bool Undo() override;

private:
	SceneBase* m_scene;
	ComponentPropertyIdentity m_identity;
	PropertyValue m_before;
	PropertyValue m_after;
};
