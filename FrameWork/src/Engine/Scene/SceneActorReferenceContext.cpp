#include "SceneActorReferenceContext.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

ActorReferenceCodecResult SceneActorReferenceContext::Validate(const Guid& guid) const
{
	Actor* actor = nullptr;
	return FindActor(guid, actor);
}

ActorReferenceCodecResult SceneActorReferenceContext::FindActor(
	const Guid& guid,
	Actor*& outActor) const
{
	Actor* actor = m_scene.ResolveActor(guid);
	if (!actor) return ActorReferenceCodecResult::ActorNotFound;
	if (actor->IsDestroyed()) return ActorReferenceCodecResult::PendingDestroy;

	outActor = actor;
	return ActorReferenceCodecResult::Success;
}
