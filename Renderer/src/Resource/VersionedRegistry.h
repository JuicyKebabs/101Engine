#pragma once

#include "ResourceSlotTable.h"

#include <algorithm>
#include <memory>
#include <limits>
#include <vector>

namespace rendering::detail
{

template <class Handle, class Content>
class VersionedRegistry
{
public:
	explicit VersionedRegistry(std::uint32_t owner) : m_slots(owner) {}

	ResourceRegistrationResult<Handle> Register(std::unique_ptr<Content> candidate)
	{
		if (!candidate)
		{
			return { { false, "Register: candidate content is null." }, {} };
		}

		const auto index = m_slots.FindRegistrationSlotIndex();

		if (index >= Handle::InvalidIndex)
		{
			return { { false, "Register: resource slot limit reached." }, {} };
		}

		// Allocate a new slot if the reusable slot is not found
		if (index >= m_contents.size())
		{
			m_contents.resize(index + 1);
		}

		auto registered = m_slots.Register();

		if (!registered.status.IsSuccess())
		{
			return { registered.status, {} };
		}

		m_contents[index] = std::move(candidate);

		return { {}, registered.handle };
	}

	Content* FindForNewUse(Handle handle) noexcept
	{
		if (m_slots.ValidateForNewUse(handle) != ResourceHandleValidationResult::Success)
		{
			return nullptr;
		}

		return m_contents[handle.slotIndex].get();
	}

	Result Update(Handle handle, std::unique_ptr<Content> candidate)
	{
		if (!candidate)
		{
			return { false, "Update: candidate content is null." };
		}

		auto* current = FindForNewUse(handle);

		if (!current || current->revision == (std::numeric_limits<std::uint64_t>::max)())
		{
			return { false, "update target/revision is unavailable" };
		}

		candidate->revision = current->revision + 1;

		// Move the current content to the retired list before updating it with the new candidate.
		m_retired.push_back(std::move(m_contents[handle.slotIndex]));
		m_contents[handle.slotIndex] = std::move(candidate);

		return {};
	}

	Result Destroy(Handle handle)
	{
		if (m_slots.RequestDestroy(handle) != ResourceHandleValidationResult::Success)
		{
			return { false, "invalid destruction handle" };
		}

		return {};
	}

	void Collect(std::uint64_t completed)
	{
		// Erase the retired handles if it's no more used by GPU (lastDrawFenceValue <= completed).
		std::erase_if(m_retired, [&](const auto& x) { return x->lastDrawFenceValue <= completed; });

		// Apply the destruction request to the contents and release the slots.
		for (std::size_t i = 0; i < m_contents.size(); ++i)
		{
			auto h = m_slots.GetDestroyRequestedHandle(i);

			if (h == Handle{} || !m_contents[i] || m_contents[i]->lastDrawFenceValue > completed)
			{
				continue;
			}

			m_contents[i].reset();

			if (m_slots.ReleaseSlotAfterResourceDestruction(h) != ResourceHandleValidationResult::Success)
			{
				std::terminate();
			}
		}
	}

private:
	ResourceSlotTable<Handle> m_slots;
	std::vector<std::unique_ptr<Content>> m_contents;

	// Save changed contents before applying changes to keep un-changed policy in frame resources.
	std::vector<std::unique_ptr<Content>> m_retired;
};

}