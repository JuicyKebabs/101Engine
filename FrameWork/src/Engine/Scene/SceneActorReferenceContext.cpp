#include "SceneActorReferenceContext.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

bool SceneActorReferenceContext::Validate(const Guid& guid) const
{
	Actor* actor = nullptr;
	return FindActor(guid, actor);
}

bool SceneActorReferenceContext::FindActor(
	const Guid& guid,
	Actor*& outActor) const
{
	Actor* actor = m_scene.ResolveActor(guid);

	if (!actor)
	{
		DBG("Actor reference: ActorNotFound.");
		return false;
	}

	if (actor->IsDestroyed())
	{
		DBG("Actor reference: PendingDestroy.");
		return false;
	}

	outActor = actor;
	return true;
}
