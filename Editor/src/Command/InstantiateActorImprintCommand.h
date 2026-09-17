#pragma once

#include "Command/IEditorCommand.h"
#include "Engine/ActorImprint/ActorImprintInstanceSnapshot.h"
#include "Engine/Core/GUID/Guid.h"

#include <string>

class ActorImprintSystem;
class SceneBase;

// Scene-document command. Initial Execute creates a fresh Instance; Redo uses
// the ET-12 snapshot so every Actor GUID and the external-parent identity are
// restored instead of generating a semantically different Instance.
class InstantiateActorImprintCommand final : public IEditorCommand
{
public:
	InstantiateActorImprintCommand(
		SceneBase& scene,
		ActorImprintSystem& system,
		Guid assetGuid,
		Guid externalParentGuid = {});

	bool Execute() override;
	bool Undo() override;
	const Guid& GetRootActorGuid() const { return m_rootActorGuid; }

private:
	SceneBase* m_scene;
	ActorImprintSystem* m_system;
	Guid m_assetGuid;
	Guid m_externalParentGuid;
	Guid m_rootActorGuid;
	ActorImprintInstanceSnapshot m_snapshot;
	bool m_hasExecuted = false;
};
