#include "ActorReference.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

bool ActorReference::Set(Actor* actor)
{
	if (!actor)
	{
		Clear();
		return true;
	}

	if (actor->IsDestroyed()) return false;

	const Guid& actorId = actor->GetGuid();
	if (!actorId.IsValid()) return false;

	m_actorId = actorId;
	m_cachedHandle = actor->GetHandle();

	return true;
}

bool ActorReference::SetGuid(const Guid& actorId)
{
	if (!actorId.IsValid()) return false;

	m_actorId = actorId;

	// Still clear the cached handle
	m_cachedHandle = ActorHandle::Null();

	return true;
}

void ActorReference::Clear()
{
	m_actorId = {};
	m_cachedHandle = ActorHandle::Null();
}

Actor* ActorReference::Resolve(SceneBase& scene) const
{
	if (!m_actorId.IsValid()) return nullptr;

	// Try the cached runtime handle first
	if (!m_cachedHandle.IsNull())
	{
		// Resolve actor from the cached handle
		Actor* cachedActor = scene.ResolveActor(m_cachedHandle);

		// Validate the cached actor and its GUID
		if (cachedActor &&
			!cachedActor->IsDestroyed() &&
			cachedActor->GetGuid() == m_actorId)
		{
			return cachedActor;
		}

		// Failed to resolve from the cached handle, clear it for the next attempt.
		// The slot was released, its generation changed, the Actor belongs
		// to another Scene, or the Actor is pending destruction.
		m_cachedHandle = ActorHandle::Null();
	}

	// Reacquire the current runtime handle from the persistent Guid
	Actor* resolvedActor = scene.ResolveActor(m_actorId);

	if (!resolvedActor || resolvedActor->IsDestroyed())
	{
		return nullptr;
	}

	// Cache the resolved handle for future fast access
	m_cachedHandle = resolvedActor->GetHandle();

	return resolvedActor;
}
