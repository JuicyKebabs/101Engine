#include "DeleteActorImprintInstanceCommand.h"
#include "Engine/Core/Debug/Debug.h"

#include "Engine/Actor/Actor.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"

DeleteActorImprintInstanceCommand::DeleteActorImprintInstanceCommand(
	SceneBase& scene,
	ActorImprintSystem& system,
	Guid rootGuid)
	: m_scene(&scene), m_system(&system), m_rootGuid(rootGuid)
{}

bool DeleteActorImprintInstanceCommand::Execute()
{
	if (!m_scene || !m_system || !m_rootGuid.IsValid())
	{
		DBG("Structural operation rejected: InvalidInstance.");
		return false;
	}

	Actor* root = m_scene->ResolveActor(m_rootGuid);

	if (!root || root->IsDestroyed())
	{
		DBG("Structural operation rejected: InvalidActor.");
		return false;
	}

	const auto* member = m_scene->GetImprintInstances().FindMember(root->GetHandle());

	if (!member || member->root != root->GetHandle())
	{
		DBG("Structural operation rejected: InstanceDestroyRequired.");
		return false;
	}

	if (!m_scene->CanDestroy(root, true))
	{
		return false;
	}

	if (!m_snapshot.GetRecord() && !m_snapshot.Capture(*m_scene, root->GetHandle()))
	{
		DBG("Structural operation rejected: InstanceSnapshotRequired.");
		return false;
	}

	return m_system->DestroyInstance(*m_scene, root->GetHandle());
}

bool DeleteActorImprintInstanceCommand::Undo()
{
	if (!m_scene || !m_system || !m_snapshot.GetRecord())
	{
		DBG("Structural operation rejected: InstanceSnapshotRequired.");
		return false;
	}

	if (m_scene->ResolveActor(m_rootGuid))
	{
		DBG("Structural operation rejected: PendingDestroy.");
		return false;
	}

	Actor* restored = m_snapshot.Restore(*m_scene, *m_system);

	if (!restored || restored->GetGuid() != m_rootGuid)
	{
		DBG("Structural operation rejected: InstanceSnapshotRequired.");
		return false;
	}

	return true;
}
