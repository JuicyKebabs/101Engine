#pragma once

#include "IEditorCommand.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Core/GUID/Guid.h"

class SceneBase;

class ChangeActorTagCommand final : public IEditorCommand
{
public:
	ChangeActorTagCommand(SceneBase* scene, const Guid& actorGuid, TagId newTag);

	bool Execute() override;
	bool Undo() override;

private:
	SceneBase* m_scene = nullptr;
	Guid m_actorGuid;
	TagId m_oldTag = TAG_NONE;
	TagId m_newTag = TAG_NONE;
	bool m_hasExecuted = false;
};
