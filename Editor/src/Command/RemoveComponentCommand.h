#pragma once
#include "IEditorCommand.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Scene/ComponentSnapshot.h"

#include <cstddef>
#include <string>

class Actor;
class Component;
class SceneBase;

//------------------------------------------------------------------
// RemoveComponentCommand class
// Command to remove a component from an actor in the scene
// Stores the necessary information to execute and undo the command
//------------------------------------------------------------------

class RemoveComponentCommand : public IEditorCommand
{
public:
	RemoveComponentCommand(
		SceneBase* scene,
		const Guid& actorGuid,
		const std::string& componentName,
		std::size_t occurrenceIndex
	);

	bool Execute() override;
	bool Undo() override;

private:
	SceneBase* m_scene = nullptr;
	Guid m_actorGuid;
	std::string m_componentName;
	std::size_t m_occurrenceIndex = 0;		// Index of the component which is duplicated in the actor by same type

	ComponentSnapshot m_componentSnapshot;	// Snapshot of the component to be removed

	bool m_hasSnapshot = false;
	bool m_isRemoved = false;

private:
	Actor* ResolveActor() const;
	Component* ResolveComponent(Actor* actor) const;
};
