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
// AddComponentCommand class
// Command to add a component to an actor in the scene
// Stores the necessary information to execute and undo the command
//------------------------------------------------------------------

class AddComponentCommand : public IEditorCommand
{
public:
	AddComponentCommand(
		SceneBase* scene,
		const Guid& actorGuid,
		const std::string& componentName
	);

	bool Execute() override;
	bool Undo() override;

private:
	SceneBase* m_scene;
	Guid m_actorGuid;
	std::string m_componentName;

	std::size_t m_occurrenceIndex = 0;		// Index of the component which is duplicated in the actor by same type
	ComponentSnapshot m_componentSnapshot;	// Snapshot of the component to be added

	bool m_hasExecuted = false;
	bool m_isApplied = false;

private:
	// Resolve actor from scene by its GUID
	Actor* ResolveActor() const;

	// Resolve component from actor by its name
	Component* ResolveComponent(Actor* actor) const;
	
};
