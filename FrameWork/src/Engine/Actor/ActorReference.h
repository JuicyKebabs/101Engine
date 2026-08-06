#pragma once
#include "Engine/Actor/ActorHandle.h"
#include "Engine/Core/GUID/Guid.h"

class Actor;
class SceneBase;

//---------------------------------------------------------------------------------------
// ActorReference class
// This class combines a persistent Actor Guid with a cached runtime ActorHandle.
// The Guid allows the reference to reconnect after the Actor is recreated by Undo/Redo.
//---------------------------------------------------------------------------------------

class ActorReference
{
public:
	// Set the referenced Actor
	// Passing nullptr will clear the reference
	bool Set(Actor* actor);

	// Set a persistent Guid without requiring the Actor to exist yet.
	bool SetGuid(const Guid& guid);

	void Clear();

	bool HasValue() const { return m_actorId.IsValid(); }

	const Guid& GetGuid() const { return m_actorId; }

	// Resolve the Actor from the cached handle or persistent Guid.
	// Returns nullptr while the Actor is missing or pending destruction.
	Actor* Resolve(SceneBase& scene) const;

private:
	// The unique identifier of the actor
	Guid m_actorId;

	// Runtime cached handle to the Actor
	mutable ActorHandle m_cachedHandle = ActorHandle::Null();
};
