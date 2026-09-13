#include "Core/EditorSelection.h"

#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

Actor* EditorSelection::ResolveActor(SceneBase* scene)
{
	if (!scene || !m_selectedActorGuid.IsValid()) return nullptr;

	Actor* actor = scene->ResolveActor(m_selectedActorGuid);
	if (!actor || actor->IsDestroyed())
	{
		Clear();
		return nullptr;
	}

	return actor;
}

void EditorSelection::Revalidate(SceneBase* scene)
{
	if (!m_selectedActorGuid.IsValid()) return;
	ResolveActor(scene);
}
