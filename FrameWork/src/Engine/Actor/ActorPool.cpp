#include "ActorPool.h"
#include "Actor.h"

ActorHandle ActorPool::Register(std::unique_ptr<Actor> actor)
{
	if (!actor) return ActorHandle::Null();

	uint32_t index;

	if (!m_freeIndices.empty())
	{
		index = m_freeIndices.back();
		m_freeIndices.pop_back();
	}
	else
	{
		index = static_cast<uint32_t>(m_slots.size());
		m_slots.push_back({ nullptr, 0, false });
	}

	Slot& slot = m_slots[index];
	ActorHandle handle = {index, slot.generation};
	actor->SetHandle(handle);

	slot.actor = std::move(actor);
	slot.pendingDestroy = false;

	return handle;
}

void ActorPool::Destroy(ActorHandle handle)
{
	if (!IsValid(handle)) return;
	m_slots[handle.index].pendingDestroy = true;

	// Destroy child actors recursively

}

Actor* ActorPool::Resolve(ActorHandle handle) const
{
	if (!IsValid(handle)) return nullptr;
	return m_slots[handle.index].actor.get();
}

bool ActorPool::IsValid(ActorHandle handle) const
{
	return handle.index < m_slots.size()
		&& m_slots[handle.index].generation == handle.generation
		&& m_slots[handle.index].actor != nullptr;
}

void ActorPool::CollectGarbage()
{
	for (size_t i = 0; i < m_slots.size(); ++i)
	{
		Slot& slot = m_slots[i];
		if (!slot.pendingDestroy || !slot.actor) continue;

		slot.actor->OnDestroy();
		slot.actor.reset();	// Release the actor instance
		slot.generation++;	// Increment generation to invalidate old handles
		slot.pendingDestroy = false;
		m_freeIndices.push_back(static_cast<uint32_t>(i));
	}
}

void ActorPool::ForEach(const std::function<void(Actor*)>& fn) const
{
	// NOTE : Do not exclude actors that are pending destruction
	// to make Collector::CollectGarbage() has a responsibility to clean up the actors.
	// Snapshot the slot count. Actors registered by the callback are processed
	// from the next traversal, and holes do not hide later live slots.
	size_t count = m_slots.size();
	for (size_t i = 0; i < count; ++i)
	{
		if (m_slots[i].actor)
		{
			fn(m_slots[i].actor.get());
		}
	}
}

size_t ActorPool::Count() const
{
	size_t count = 0;
	for (const auto& slot : m_slots)
	{
		if (slot.actor) count++;
	}
	return count;
}
