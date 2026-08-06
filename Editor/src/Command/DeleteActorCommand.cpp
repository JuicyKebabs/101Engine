#include "DeleteActorCommand.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/ActorSubtreeRestorer.h"
#include "Engine/Scene/SceneBase.h"

DeleteActorCommand::DeleteActorCommand(
	SceneBase* scene,
	const Guid& actorGuid
)
	: m_scene(scene), m_actorGuid(actorGuid)
{}

bool DeleteActorCommand::Execute()
{
	if (!m_scene || !m_actorGuid.IsValid()) return false;

	// Resolve the actor by its Guid
	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	if (!actor || actor->IsDestroyed()) return false;

	if (!m_hasSnapshot)
	{
		// Capture a snapshot of the actor subtree before deletion
		if (!m_snapshot.Capture(actor, m_scene)) return false;

		m_hasSnapshot = true;
	}

	// Remove the actor subtree from the scene
	m_scene->RemoveActor(actor, /*cascadeToChildren=*/true);

	// Verify that the actor is marked as destroyed
	return actor->IsDestroyed();
}

bool DeleteActorCommand::Undo()
{
	// Ensure that the scene, snapshot, and snapshot validity are all present
	if (!m_scene ||
		!m_hasSnapshot ||
		!m_snapshot.IsValid())
	{
		return false;
	}

	// Check if the actor with the same Guid already exists in the scene
	if (m_scene->ResolveActor(m_actorGuid)) return false;

	// Restore the actor subtree from the snapshot
	Actor* restoredRoot = ActorSubtreeRestorer::Restore(m_snapshot, m_scene);

	return restoredRoot != nullptr;
}
