#pragma once
#include "IEditorCommand.h"
#include "Engine/Core/GUID/Guid.h"
#include <string>

class SceneBase;

class RenameActorCommand : public IEditorCommand
{
public:
	RenameActorCommand(SceneBase* scene, const Guid& actorGuid, const std::string& newName);

	bool Execute() override;
	bool Undo() override;

private:
	SceneBase* m_pScene = nullptr;

	Guid m_actorGuid;

	std::string m_oldName;
	std::string m_newName;

	bool m_hasExecuted = false;
};