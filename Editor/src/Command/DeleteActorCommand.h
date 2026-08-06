#pragma once
#include "IEditorCommand.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Scene/ActorSubtreeSnapshot.h"

class SceneBase;

//------------------------------------------------------------------------------------------
// DeleteActorCommand class
// Deletes an Actor subtree while preserving a serialized snapshot for Undo/Redo.
// Actor identity is stored as a Guid so the command never retains a dangling Actor pointer.
//------------------------------------------------------------------------------------------

class DeleteActorCommand : public IEditorCommand
{
public:
	DeleteActorCommand(SceneBase* scene, const Guid& actorGuid);

	bool Execute() override;
	bool Undo() override;

	const Guid& GetActorGuid() const { return m_actorGuid; }

private:
	// The Scene in which the Actor exists
	SceneBase* m_scene = nullptr;

	// The Guid of the Actor to delete (used for Undo/Redo)
	Guid m_actorGuid;

	// Snapshot of the Actor subtree to restore on Undo
	ActorSubtreeSnapshot m_snapshot;

	// Flag indicating whether the snapshot has been created
	bool m_hasSnapshot = false;
};
