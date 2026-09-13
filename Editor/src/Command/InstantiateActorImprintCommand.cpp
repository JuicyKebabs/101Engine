#include "InstantiateActorImprintCommand.h"

#include "Engine/Actor/Actor.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"

#include <utility>

InstantiateActorImprintCommand::InstantiateActorImprintCommand(
	SceneBase& scene, ActorImprintSystem& system, Guid assetGuid, Guid externalParentGuid)
	: m_scene(&scene), m_system(&system), m_assetGuid(assetGuid),
	  m_externalParentGuid(externalParentGuid)
{}

bool InstantiateActorImprintCommand::Execute()
{
	m_structuralResult = {};
	m_errorMessage.clear();
	if (!m_scene || !m_system || !m_assetGuid.IsValid())
	{
		m_structuralResult.reason = StructuralMutationReason::InvalidInstance;
		m_errorMessage = "ActorImprint command context or AssetGUID is invalid.";
		return false;
	}

	ActorHandle parentHandle;
	if (m_externalParentGuid.IsValid())
	{
		Actor* parent = m_scene->ResolveActor(m_externalParentGuid);
		if (!parent || parent->IsDestroyed() || parent->GetOwner() != m_scene)
		{
			m_structuralResult.reason = StructuralMutationReason::InvalidActor;
			m_errorMessage = "Drop parent is not a live Actor in the destination Scene.";
			return false;
		}
		if (m_scene->GetImprintInstances().FindMember(parent->GetHandle()))
		{
			m_structuralResult.reason = StructuralMutationReason::ImprintMemberImmutable;
			m_errorMessage = "ActorImprint Instances cannot be used as drop parents.";
			return false;
		}
		parentHandle = parent->GetHandle();
	}

	Actor* root = nullptr;
	if (!m_hasExecuted)
	{
		ActorImprintLoadError loadError;
		const ActorImprintHandle imprint = m_system->Load(m_assetGuid, &loadError);
		if (imprint.IsNull())
		{
			m_structuralResult.reason = StructuralMutationReason::InvalidInstance;
			m_errorMessage = loadError.message;
			return false;
		}
		ActorImprintMaterializationError error;
		root = m_system->Instantiate(*m_scene, imprint, parentHandle, &error);
		if (!root)
		{
			m_structuralResult.reason = error.code == ActorImprintMaterializationErrorCode::InvalidParent
				? StructuralMutationReason::InvalidActor : StructuralMutationReason::InvalidInstance;
			m_errorMessage = std::move(error.message);
			return false;
		}
		ActorImprintInstanceSerializationError snapshotError;
		if (!m_snapshot.Capture(*m_scene, root->GetHandle(), &snapshotError))
		{
			m_system->DestroyInstance(*m_scene, root->GetHandle(), &m_structuralResult);
			m_errorMessage = snapshotError.message;
			return false;
		}
		m_rootActorGuid = root->GetGuid();
		m_hasExecuted = true;
		return true;
	}

	ActorImprintInstanceDeserializationError restoreError;
	root = m_snapshot.Restore(*m_scene, *m_system, &restoreError);
	if (!root)
	{
		m_structuralResult.reason = StructuralMutationReason::InstanceSnapshotRequired;
		m_errorMessage = std::move(restoreError.message);
		return false;
	}
	if (root->GetGuid() != m_rootActorGuid)
	{
		m_structuralResult.reason = StructuralMutationReason::InstanceSnapshotRequired;
		m_errorMessage = "Restored Instance root identity changed.";
		return false;
	}
	return true;
}

bool InstantiateActorImprintCommand::Undo()
{
	m_structuralResult = {};
	m_errorMessage.clear();
	if (!m_hasExecuted || !m_scene || !m_system || !m_rootActorGuid.IsValid()) return false;
	Actor* root = m_scene->ResolveActor(m_rootActorGuid);
	if (!root || root->IsDestroyed())
	{
		m_structuralResult.reason = StructuralMutationReason::InvalidInstance;
		m_errorMessage = "Created ActorImprint Instance is no longer live.";
		return false;
	}
	if (!m_system->DestroyInstance(*m_scene, root->GetHandle(), &m_structuralResult))
	{
		m_errorMessage = "ActorImprint Instance could not be destroyed as one unit.";
		return false;
	}
	return true;
}
