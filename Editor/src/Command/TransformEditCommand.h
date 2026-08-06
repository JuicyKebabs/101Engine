#pragma once
#include "IEditorCommand.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Core/Math/Math.h"

class SceneBase;

//------------------------------------------------------------------
// TransformEditCommand class
// Command to edit the transform of an actor in the scene
// Stores the necessary information to execute and undo the command
//------------------------------------------------------------------

class TransformEditCommand : public IEditorCommand
{
public:
	TransformEditCommand(
		SceneBase* scene,
		const Guid& actorGuid,
		const Transform3D& before,
		const Transform3D& after
	);

	bool Execute() override;
	bool Undo() override;

private:
	bool Apply(const Transform3D& state);

	SceneBase* m_scene = nullptr;
	Guid m_actorGuid;
	Transform3D m_before;
	Transform3D m_after;
};
