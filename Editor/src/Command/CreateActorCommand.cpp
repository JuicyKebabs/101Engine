#include "CreateActorCommand.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Scene/SceneBase.h"

CreateActorCommand::CreateActorCommand(SceneBase* scene, const Actor::InitDesc& desc)
	: m_scene(scene), m_desc(desc)
{}

bool CreateActorCommand::Execute()
{
	if (!m_scene) return false;

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

	if (!m_scene->AddRootActor(std::move(actor))) return false;

	m_hasExecuted = true;
	return true;
}

bool CreateActorCommand::Undo()
{
	if (!m_scene) return false;

	// Resolve the actor using the stored GUID
	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	if (!actor || actor->IsDestroyed()) return false;

	// Remove the actor from the scene
	m_scene->RemoveActor(actor);

	return true;
}