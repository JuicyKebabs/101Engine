#include "InstantiateActorImprintCommand.h"
#include "Engine/Core/Debug/Debug.h"

#include "Engine/Actor/Actor.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"

#include <utility>

InstantiateActorImprintCommand::InstantiateActorImprintCommand(
	SceneBase& scene,
	ActorImprintSystem& system,
	Guid assetGuid,
	Guid externalParentGuid)
	: m_scene(&scene), m_system(&system), m_assetGuid(assetGuid),
	  m_externalParentGuid(externalParentGuid)
{}

bool InstantiateActorImprintCommand::Execute()
{
	if (!m_scene || !m_system || !m_assetGuid.IsValid())
	{
		DBG("ActorImprint command context or AssetGUID is invalid.");
		return false;
	}

	ActorHandle parentHandle;

	if (m_externalParentGuid.IsValid())
	{
		Actor* parent = m_scene->ResolveActor(m_externalParentGuid);

		if (!parent || parent->IsDestroyed() || parent->GetOwner() != m_scene)
		{
			DBG("Drop parent is not a live Actor in the destination Scene.");
			return false;
		}

		if (m_scene->GetImprintInstances().FindMember(parent->GetHandle()))
		{
			DBG("ActorImprint Instances cannot be used as drop parents.");
			return false;
		}

		parentHandle = parent->GetHandle();
	}

	Actor* root = nullptr;

	if (!m_hasExecuted)
	{
		const ActorImprintHandle imprint = m_system->Load(m_assetGuid);

		if (imprint.IsNull())
		{
			return false;
		}

		root = m_system->Instantiate(*m_scene, imprint, parentHandle);

		if (!root)
		{
			return false;
		}

		if (!m_snapshot.Capture(*m_scene, root->GetHandle()))
		{
			m_system->DestroyInstance(*m_scene, root->GetHandle());

			return false;
		}

		m_rootActorGuid = root->GetGuid();
		m_hasExecuted = true;
		return true;
	}

	root = m_snapshot.Restore(*m_scene, *m_system);

	if (!root)
	{
		return false;
	}

	if (root->GetGuid() != m_rootActorGuid)
	{
		DBG("Restored Instance root identity changed.");
		return false;
	}

	return true;
}

bool InstantiateActorImprintCommand::Undo()
{
	if (!m_hasExecuted || !m_scene || !m_system || !m_rootActorGuid.IsValid())
	{
		return false;
	}

	Actor* root = m_scene->ResolveActor(m_rootActorGuid);

	if (!root || root->IsDestroyed())
	{
		DBG("Created ActorImprint Instance is no longer live.");
		return false;
	}

	if (!m_system->DestroyInstance(*m_scene, root->GetHandle()))
	{
		DBG("ActorImprint Instance could not be destroyed as one unit.");
		return false;
	}

	return true;
}
