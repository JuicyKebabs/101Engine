#pragma once
#include "ActorHandle.h"
#include <vector>
#include <memory>
#include <functional>

//---------------------------------------------------------------------------------------
// ActorPool class
// This class has all ownership of Actor instances in the scene.
// It manages the lifetime of actors and provides a way to resolve ActorHandle to Actor*.
//---------------------------------------------------------------------------------------

class Actor;

class ActorPool
{
public:
	// Take ownership of actor from Factroy and return a handle for it.
	ActorHandle Register(std::unique_ptr<Actor> actor);

	// Mark the actor for deferred destruction. The actor remains resolvable
	// until CollectGarbage() so references stay stable for the current frame.
	void Destroy(ActorHandle);

	// Resolve a handle to its Actor pointer.
	// Check that the generation matches and the slot still owns an actor.
	Actor* Resolve(ActorHandle handle) const;

	bool IsValid(ActorHandle handle) const;

	// Calls OnDestroy and releases actors marked for destruction at frame end.
	// Generation for destroyed Actor is incremented to avoid dangling references.
	void CollectGarbage();

	// Iterate all available actors in the pool and apply a function to them.
	// Used by ScaneBase for Actor iteration.
	void ForEach(const std::function<void(Actor*)>& fn) const;

	size_t Count() const;

private:
	struct Slot
	{
		std::unique_ptr<Actor> actor;	// The actor instance in this slot
		uint32_t generation = 0;		// Generation counter for this slot, incremented on destruction
		bool pendingDestroy = false;	// Flag indicating if the actor is marked for destruction
	};

	std::vector<Slot> m_slots;				// The pool of actor slots
	std::vector<uint32_t> m_freeIndices;	// Indices of free slots in the pool
};
