#include "DeleteActorImprintInstanceCommand.h"

#include "Engine/Actor/Actor.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"

DeleteActorImprintInstanceCommand::DeleteActorImprintInstanceCommand(
	SceneBase& scene, ActorImprintSystem& system, Guid rootGuid)
	: m_scene(&scene), m_system(&system), m_rootGuid(rootGuid)
{}

bool DeleteActorImprintInstanceCommand::Execute()
{
	m_structuralResult = {};
	if (!m_scene || !m_system || !m_rootGuid.IsValid())
		return StructuralMutationResult{ StructuralMutationReason::InvalidInstance }.Report(&m_structuralResult);
	Actor* root = m_scene->ResolveActor(m_rootGuid);
	if (!root || root->IsDestroyed())
		return StructuralMutationResult{ StructuralMutationReason::InvalidActor }.Report(&m_structuralResult);
	const auto* member = m_scene->GetImprintInstances().FindMember(root->GetHandle());
	if (!member || member->root != root->GetHandle())
		return StructuralMutationResult{ StructuralMutationReason::InstanceDestroyRequired }.Report(&m_structuralResult);
	if (!m_scene->CanDestroy(root, true).Report(&m_structuralResult)) return false;
	if (!m_snapshot.GetRecord() && !m_snapshot.Capture(*m_scene, root->GetHandle()))
		return StructuralMutationResult{ StructuralMutationReason::InstanceSnapshotRequired }.Report(&m_structuralResult);
	return m_system->DestroyInstance(*m_scene, root->GetHandle(), &m_structuralResult);
}

bool DeleteActorImprintInstanceCommand::Undo()
{
	m_structuralResult = {};
	if (!m_scene || !m_system || !m_snapshot.GetRecord())
		return StructuralMutationResult{ StructuralMutationReason::InstanceSnapshotRequired }.Report(&m_structuralResult);
	if (m_scene->ResolveActor(m_rootGuid))
		return StructuralMutationResult{ StructuralMutationReason::PendingDestroy }.Report(&m_structuralResult);
	Actor* restored = m_snapshot.Restore(*m_scene, *m_system);
	if (!restored || restored->GetGuid() != m_rootGuid)
		return StructuralMutationResult{ StructuralMutationReason::InstanceSnapshotRequired }.Report(&m_structuralResult);
	return true;
}
