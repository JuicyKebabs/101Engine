#pragma once

#include "ActorImprintEditingObjectMap.h"
#include "Command/IEditorCommand.h"

#include <memory>

class SceneBase;

// Keeps a reused Editor command and the LocalObjectID map in the same history
// unit. Snapshots contain stable locators rather than Component pointers, so an
// Undo that reconstructs Components can safely bind their original IDs.
class ActorImprintEditingCommand final : public IEditorCommand
{
public:
	ActorImprintEditingCommand(
		SceneBase& scene,
		ActorImprintEditingObjectMap& objectMap,
		std::unique_ptr<IEditorCommand> command);

	bool Execute() override;
	bool Undo() override;

private:
	SceneBase* m_scene;
	ActorImprintEditingObjectMap* m_objectMap;
	std::unique_ptr<IEditorCommand> m_command;
	ActorImprintEditingObjectMap::Snapshot m_before;
	ActorImprintEditingObjectMap::Snapshot m_after;
	bool m_hasExecuted = false;
};
