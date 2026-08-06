#pragma once
#include "IEditorCommand.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Scene/TransformSubtreeSnapshot.h"

//-------------------------------------------------------------------------------------------
// ReparentActorCommand class
// Changes the parent of an Actor while preserving the exact Transform-family state
// of its subtree for Undo and Redo
// Actor identity is stored by Guid to avoid dangling pointer due to storing Actor* directly
//-------------------------------------------------------------------------------------------

class Actor;
class SceneBase;

class ReparentActorCommand : public IEditorCommand
{
public:
	ReparentActorCommand(
		SceneBase* scene,
		const Guid& actorGuid,
		const Guid& newParentGuid
	);

	bool Execute() override;
	bool Undo() override;

	const Guid& GetActorGuid() const { return m_actorGuid; }
	const Guid& GetOldParentGuid() const { return m_oldParentGuid; }
	const Guid& GetNewParentGuid() const { return m_newParentGuid; }

private:
	SceneBase* m_scene = nullptr;

	// Actor whose parent is changed
	Guid m_actorGuid;

	// Guid of the old and new parent Actor
	Guid m_oldParentGuid;	// Parent before the first execution
	Guid m_newParentGuid;	// Parent after the first execution

	// Exact Transform-family states before and after the first execution
	TransformSubtreeSnapshot m_beforeSnapshot;
	TransformSubtreeSnapshot m_afterSnapshot;

	// Distinguishes the initial execution from Redo
	bool m_hasExecuted = false;

private:
	// Resolve a parent Guid.
	// An invalid Guid represents the root hierarchy and resolves successfully to nullptr.
	bool ResolveParent(const Guid& parentGuid, Actor*& outParent) const;

	// Check whether the Actor currently belongs to the expected parent.
	bool HasExpectedParent(const Actor* actor, const Guid& expectedParentGuid) const;

	// Apply a stored hierarchy and Transform state.
	// If application fails, restore the rollback hierarchy and snapshot.
	bool ApplyStoredState(
		Actor* actor,
		const Guid& targetParentGuid,
		const TransformSubtreeSnapshot& targetSnapshot,
		const Guid& rollbackParentGuid,
		const TransformSubtreeSnapshot& rollbackSnapshot
	);

	// Best-effort rollback used after a partially applied operation.
	bool RollbackState(
		Actor* actor,
		const Guid& parentGuid,
		const TransformSubtreeSnapshot& snapshot
	);
};
