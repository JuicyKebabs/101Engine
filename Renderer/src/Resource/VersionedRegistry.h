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
	std::string DescribeUnavailable(Handle handle) const
	{
		return DescribeResourceIdentity(handle, m_slots.ValidateForNewUse(handle));
	}

	explicit VersionedRegistry(std::uint32_t owner) : m_slots(owner) {}

	ResourceRegistrationResult<Handle> Register(std::unique_ptr<Content> candidate)
	{
		if (!candidate)
		{
			return {{false, "Register: candidate content is null."}, {}};
		}

		const auto index = m_slots.FindRegistrationSlotIndex();

		if (index >= Handle::InvalidIndex)
		{
			return {{false, "Register: resource slot limit reached."}, {}};
		}

		// Allocate a new slot if the reusable slot is not found
		if (index >= m_contents.size())
		{
			m_contents.resize(index + 1);
		}

		auto registered = m_slots.Register();

		if (!registered.status.IsSuccess())
		{
			return {registered.status, {}};
		}

		m_contents[index] = std::move(candidate);

		return {{}, registered.handle};
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
			return {false, "Update: candidate content is null."};
		}

		auto* current = FindForNewUse(handle);

		if (!current || current->revision == (std::numeric_limits<std::uint64_t>::max)())
		{
			return {false, "update target/revision is unavailable"};
		}

		candidate->revision = current->revision + 1;

		// Public updates occur after GPU completion and outside Render borrows.
		m_contents[handle.slotIndex] = std::move(candidate);

		return {};
	}

	Result Destroy(Handle handle)
	{
		if (m_slots.RequestDestroy(handle) != ResourceHandleValidationResult::Success)
		{
			return {false, "invalid destruction handle"};
		}

		m_contents[handle.slotIndex].reset();

		if (m_slots.ReleaseSlotAfterResourceDestruction(handle) != ResourceHandleValidationResult::Success)
		{
			std::terminate();
		}

		return {};
	}

private:
	ResourceSlotTable<Handle> m_slots;
	std::vector<std::unique_ptr<Content>> m_contents;
};

} // namespace rendering::detail