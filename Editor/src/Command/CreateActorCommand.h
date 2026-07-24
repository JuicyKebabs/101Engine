#pragma once
#include "IEditorCommand.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/GUID/Guid.h"

//--------------------------------------------------------------------------------------------
// CreateActorCommand class
// A command to create an actor in the scene. 
// It implements the IEditorCommand interface, allowing it to be executed, undone, and redone.
// --------------------------------------------------------------------------------------------
class SceneBase;

class CreateActorCommand : public IEditorCommand
{
public:
	CreateActorCommand(SceneBase* scene, const Actor::InitDesc& desc);

	bool Execute() override;
	bool Undo() override;

	const Guid& GetActorGuid() const { return m_actorGuid; }

private:
	SceneBase* m_scene = nullptr;
	Actor::InitDesc m_desc;

	Guid m_actorGuid{};
	bool m_hasExecuted = false;
};

