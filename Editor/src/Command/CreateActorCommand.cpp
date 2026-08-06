#include "CreateActorCommand.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Scene/SceneBase.h"

CreateActorCommand::CreateActorCommand(SceneBase* scene, const Actor::InitDesc& desc, Guid parentGuid)
	: m_pScene(scene), m_desc(desc), m_parentGuid(parentGuid)
{}

bool CreateActorCommand::Execute()
{
	if (!m_pScene) return false;

	std::unique_ptr<Actor> actor;

	if (!m_hasExecuted)
	{// Create new actor only if this command has not been executed before
		actor = ActorFactory::CreateEmptyActor(m_desc);

		if (!actor) return false;

		m_actorGuid = actor->GetGuid();
	}
	else
	{// Restore the actor if this command has been executed before(in case of redo)
		actor = ActorFactory::RestoreEmptyActor(m_desc, m_actorGuid);

		if (!actor) return false;
	}

	Actor* parentActor = nullptr;

	// If a parent GUID is provided, resolve it to an actor and add the new actor as a child
	if (m_parentGuid.IsValid())
	{
		parentActor = m_pScene->ResolveActor(m_parentGuid);

		if (!parentActor ||
			parentActor->IsDestroyed() ||
			parentActor->GetOwner() != m_pScene)
		{
			return false;
		}

		if (!m_pScene->AddChildActor(std::move(actor), parentActor->GetHandle())) return false;

		m_hasExecuted = true;
		return true;
	}

	// If no parent is specified, add the new actor as a root actor in the scene
	if (!m_pScene->AddRootActor(std::move(actor))) return false;

	m_hasExecuted = true;
	return true;
}

bool CreateActorCommand::Undo()
{
	if (!m_pScene) return false;

	// Resolve the actor using the stored GUID
	Actor* actor = m_pScene->ResolveActor(m_actorGuid);

	if (!actor || actor->IsDestroyed()) return false;

	// Remove the actor from the scene
	m_pScene->RemoveActor(actor);

	return true;
}
