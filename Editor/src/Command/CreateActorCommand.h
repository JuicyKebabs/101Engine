#pragma once
#include "IEditorCommand.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/GUID/Guid.h"

class SceneBase;

//--------------------------------------------------------------------------------------------
// CreateActorCommand class
// A command to create an actor in the scene. 
// It implements the IEditorCommand interface, allowing it to be executed, undone, and redone.
// --------------------------------------------------------------------------------------------

class CreateActorCommand : public IEditorCommand
{
public:
	CreateActorCommand(
		SceneBase* scene,
		const Actor::InitDesc& desc,
		Guid parentGuid = {}
	);

	bool Execute() override;
	bool Undo() override;

	const Guid& GetActorGuid() const { return m_actorGuid; }

private:
	SceneBase* m_pScene = nullptr;
	Actor::InitDesc m_desc;

	Guid m_actorGuid{};		// The GUID of the actor to be created
	Guid m_parentGuid{};	// For creating Actor as a child of a specific parent actor

	bool m_hasExecuted = false;
};

