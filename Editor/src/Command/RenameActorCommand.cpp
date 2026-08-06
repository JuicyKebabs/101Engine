#include "RenameActorCommand.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"

RenameActorCommand::RenameActorCommand(SceneBase* scene, const Guid& actorGuid, const std::string& newName)
    : m_pScene(scene), m_actorGuid(actorGuid), m_newName(newName)
{
}

bool RenameActorCommand::Execute()
{
    if (!m_pScene) return false;

    Actor* actor = m_pScene->ResolveActor(m_actorGuid);
    if (!actor ||
        actor->IsDestroyed() ||
        actor->GetOwner() != m_pScene ||
        m_newName.empty())
    {
        return false;
    }

	// First execution
    if (!m_hasExecuted)
    {
        auto oldName = actor->GetName();

        if (oldName == m_newName) return false;

        m_oldName = oldName;
        m_hasExecuted = true;
        actor->SetName(m_newName);

		return true;
    }

    // Redo
    if (actor->GetName() != m_oldName) return false;

    actor->SetName(m_newName);

    return true;
}

bool RenameActorCommand::Undo()
{
    if (!m_hasExecuted || !m_pScene) return false;

    Actor* actor = m_pScene->ResolveActor(m_actorGuid);
    if (!actor ||
        actor->IsDestroyed() ||
        actor->GetOwner() != m_pScene)
    {
        return false;
    }

    if (actor->GetName() != m_newName) return false;

    actor->SetName(m_oldName);

    return true;
}
