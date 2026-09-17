#include "ChangeActorTagCommand.h"

#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

ChangeActorTagCommand::ChangeActorTagCommand(
	SceneBase* scene,
	const Guid& actorGuid,
	TagId newTag)
	: m_scene(scene), m_actorGuid(actorGuid), m_newTag(newTag)
{}

bool ChangeActorTagCommand::Execute()
{
	if (!m_scene)
	{
		return false;
	}

	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	if (!actor || actor->IsDestroyed() || actor->GetOwner() != m_scene)
	{
		return false;
	}

	if (!m_hasExecuted)
	{
		m_oldTag = actor->GetTag();

		if (m_oldTag == m_newTag)
		{
			return false;
		}

		actor->SetTag(m_newTag);
		m_hasExecuted = true;
		return true;
	}

	if (actor->GetTag() != m_oldTag)
	{
		return false;
	}

	actor->SetTag(m_newTag);
	return true;
}

bool ChangeActorTagCommand::Undo()
{
	if (!m_scene || !m_hasExecuted)
	{
		return false;
	}

	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	if (!actor || actor->IsDestroyed() || actor->GetOwner() != m_scene ||
		actor->GetTag() != m_newTag)
	{
		return false;
	}

	actor->SetTag(m_oldTag);
	return true;
}
