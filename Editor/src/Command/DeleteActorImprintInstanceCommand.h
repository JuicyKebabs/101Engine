#pragma once

#include "Command/IEditorCommand.h"
#include "Engine/ActorImprint/ActorImprintInstanceSnapshot.h"
#include "Engine/Core/GUID/Guid.h"

class ActorImprintSystem;
class SceneBase;

// Deletes one complete ActorImprint Instance and preserves its identity for Undo/Redo.
class DeleteActorImprintInstanceCommand final : public IEditorCommand
{
public:
	DeleteActorImprintInstanceCommand(SceneBase& scene, ActorImprintSystem& system, Guid rootGuid);
	bool Execute() override;
	bool Undo() override;

private:
	SceneBase* m_scene;
	ActorImprintSystem* m_system;
	Guid m_rootGuid;
	ActorImprintInstanceSnapshot m_snapshot;
};
