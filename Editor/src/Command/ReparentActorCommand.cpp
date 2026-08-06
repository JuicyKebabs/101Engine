#include "ReparentActorCommand.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

ReparentActorCommand::ReparentActorCommand(
	SceneBase* scene,
	const Guid& actorGuid,
	const Guid& newParentGuid
)
	: m_scene(scene),
	m_actorGuid(actorGuid),
	m_newParentGuid(newParentGuid)
{
	Actor* actor = m_scene && m_actorGuid.IsValid()
		? m_scene->ResolveActor(m_actorGuid)
		: nullptr;

	if (actor && !actor->IsDestroyed() && actor->GetOwner() == m_scene)
	{
		Actor* oldParent = actor->GetParent();
		if (oldParent) m_oldParentGuid = oldParent->GetGuid();
	}
}

bool ReparentActorCommand::Execute()
{
	if (!m_scene || !m_actorGuid.IsValid()) return false;

	// Resolve the target Actor and validate it
	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	if (!actor ||
		actor->IsDestroyed() ||
		actor->GetOwner() != m_scene)
	{
		return false;
	}

	// Resolve the new parent Actor
	Actor* newParent = nullptr;
	if (!ResolveParent(m_newParentGuid, newParent)) return false;

	if (!m_hasExecuted)
	{// Initial execution
		// Check if this Actor is a child of the expected old parent
		if (!HasExpectedParent(actor, m_oldParentGuid)) return false;

		// Get the handle of the new parent Actor (or null if root Actor)
		const ActorHandle newParentHandle = newParent
			? newParent->GetHandle()
			: ActorHandle::Null();

		// No changes in parent, no need to reparent (Don't execute and stack the command)
		if (actor->GetParentHandle() == newParentHandle) return false;

		// Capture the Transform-family state of the subtree before reparenting
		if (!m_beforeSnapshot.Capture(actor, m_scene)) return false;

		// Attempt to reparent the Actor
		if (!m_scene->ReparentActor(actor, newParent))
		{// Rollback to the state which was captured before reparenting
			RollbackState(
				actor,
				m_oldParentGuid,
				m_beforeSnapshot
			);

			return false;
		}

		// Capture the Transform-family state of the subtree after reparenting successfully
		if (!m_afterSnapshot.Capture(actor, m_scene))
		{
			RollbackState(
				actor,
				m_oldParentGuid,
				m_beforeSnapshot
			);

			return false;
		}

		m_hasExecuted = true;
		return true;
	}

	// Redo execution
	// Not first execution means this execution is a Redo
	if (!HasExpectedParent(actor, m_oldParentGuid)) return false;

	// Apply the stored state after the first execution
	return ApplyStoredState(
		actor,
		m_newParentGuid,
		m_afterSnapshot,
		m_oldParentGuid,
		m_beforeSnapshot
	);
}

bool ReparentActorCommand::Undo()
{
	// Validate the command state and stored snapshots
	if (!m_scene ||
		!m_hasExecuted ||
		!m_beforeSnapshot.IsValid() ||
		!m_afterSnapshot.IsValid())
	{
		return false;
	}

	// Resolve the target Actor and validate it
	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	if (!actor ||
		actor->IsDestroyed() ||
		actor->GetOwner() != m_scene)
	{
		return false;
	}

	// Check if this Actor is a child of the expected new parent
	if (!HasExpectedParent(actor, m_newParentGuid)) return false;

	// Apply the stored state before the first execution
	return ApplyStoredState(
		actor,
		m_oldParentGuid,
		m_beforeSnapshot,
		m_newParentGuid,
		m_afterSnapshot
	);
}

bool ReparentActorCommand::ResolveParent(
	const Guid& parentGuid,
	Actor*& outParent
) const
{
	outParent = nullptr;

	if (!m_scene) return false;

	// An invalid Guid represents the root hierarchy and resolves successfully to nullptr
	if (!parentGuid.IsValid()) return true;

	// Resolve the parent Actor by its Guid and validate it
	outParent = m_scene->ResolveActor(parentGuid);

	if (!outParent ||
		outParent->IsDestroyed() ||
		outParent->GetOwner() != m_scene)
	{
		outParent = nullptr;
		return false;
	}

	return true;
}

bool ReparentActorCommand::HasExpectedParent(
	const Actor* actor,
	const Guid& expectedParentGuid
) const
{
	if (!actor || !m_scene) return false;

	if (!expectedParentGuid.IsValid())
	{
		return actor->GetParentHandle().IsNull();
	}

	// Resolve the expected parent Actor by its Guid and validate it
	Actor* expectedParent = m_scene->ResolveActor(expectedParentGuid);

	if (!expectedParent ||
		expectedParent->IsDestroyed() ||
		expectedParent->GetOwner() != m_scene)
	{
		return false;
	}

	// Check if the Actor's current parent matches the expected parent
	return actor->GetParentHandle() == expectedParent->GetHandle();
}

bool ReparentActorCommand::ApplyStoredState(
	Actor* actor,
	const Guid& targetParentGuid,
	const TransformSubtreeSnapshot& targetSnapshot,
	const Guid& rollbackParentGuid,
	const TransformSubtreeSnapshot& rollbackSnapshot
)
{
	// Validate the input parameters
	if (!actor ||
		!targetSnapshot.IsValid() ||
		!rollbackSnapshot.IsValid())
	{
		return false;
	}

	Actor* targetParent = nullptr;
	Actor* rollbackParent = nullptr;

	// Resolve the target and rollback parent Actors
	if (!ResolveParent(targetParentGuid, targetParent) ||
		!ResolveParent(rollbackParentGuid, rollbackParent))
	{
		return false;
	}

	// Attempt to reparent the Actor to the target parent
	if (!m_scene->ReparentActor(actor, targetParent))
	{// Rollback to the state given by rollbackParentGuid and rollbackSnapshot
		RollbackState(
			actor,
			rollbackParentGuid,
			rollbackSnapshot
		);

		return false;
	}

	// Attempt to restore the Transform-family state of the subtree 
	// after reparenting successfully
	if (!targetSnapshot.Restore(m_scene))
	{// Rollback to the state given by rollbackParentGuid and rollbackSnapshot
		RollbackState(
			actor,
			rollbackParentGuid,
			rollbackSnapshot
		);

		return false;
	}

	return true;
}

bool ReparentActorCommand::RollbackState(
	Actor* actor,
	const Guid& parentGuid,
	const TransformSubtreeSnapshot& snapshot
)
{
	if (!actor || !snapshot.IsValid()) return false;

	// Resolve the parent Actor used for rollback
	Actor* parent = nullptr;
	if (!ResolveParent(parentGuid, parent)) return false;

	// Rebuild hierarchy-derived state first, then restore the exact
	// Transform-family state captured before the failed operation
	const bool hierarchyRestored = m_scene->ReparentActor(actor, parent);

	const bool transformRestored = snapshot.Restore(m_scene);

	return hierarchyRestored && transformRestored;
}
