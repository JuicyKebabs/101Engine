#pragma once

#include "Renderer/Result.h"
#include "ResourceHandleValidation.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <vector>

//--------------------------------------------------------------------------------------------------------
// Resource slot table
// This class manages the registration and lifecycle of resource slots for a specific resource type.
// Template slot table for managing resource slots of a specific resource type (e.g., geometry, texture).
//--------------------------------------------------------------------------------------------------------

namespace rendering::detail
{

template<class Handle>
class ResourceSlotTable
{
public:
	struct [[nodiscard]] RegistrationResult
	{
		Result status;
		Handle handle;
	};

	explicit ResourceSlotTable(uint32_t ownerId) noexcept
		: m_ownerId(ownerId)
	{}

	ResourceSlotTable(const ResourceSlotTable&) = delete;
	ResourceSlotTable& operator=(const ResourceSlotTable&) = delete;
	ResourceSlotTable(ResourceSlotTable&&) = delete;
	ResourceSlotTable& operator=(ResourceSlotTable&&) = delete;

	[[nodiscard]]
	RegistrationResult Register()
	{
		if (m_ownerId == Handle::InvalidIndex)
		{
			return { { false, "Resource owner ID is invalid." }, {} };
		}

		// Find the smallest index of the slot that can be reused.
		std::size_t index = FindRegistrationSlotIndex();

		// In case of the reusable slot is not found
		if (index == m_slots.size())
		{
			if (index >= Handle::InvalidIndex || index >= m_slots.max_size())
			{
				return  { { false, "Resource slot limit reached." }, {} };
			}

			try
			{
				m_slots.emplace_back();
			}
			catch (const std::bad_alloc&)
			{
				return { { false, "Resource slot allocation failed." }, {} };
			}
			catch (const std::length_error&)
			{
				return { { false, "Resource slot limit reached." }, {} };
			}
		}

		ResourceSlotState& slot = m_slots[index];
		slot.registered = true;
		slot.destroyRequested = false;

		return { {}, Handle{ m_ownerId, static_cast<uint32_t>(index), slot.generation } };
	}
	
	// Requests the destruction of a registered handle.
	// The actual destruction is deferred until the GPU has finished using the resource.
	[[nodiscard]]
	ResourceHandleValidationResult RequestDestroy(Handle handle) noexcept
	{
		const auto result = ValidateRegistered(handle);

		if (result != ResourceHandleValidationResult::Success)
		{
			return result;
		}

		m_slots[handle.slotIndex].destroyRequested = true;
		return ResourceHandleValidationResult::Success;
	}

	// Releases a slot which is requested for destruction.
	// The generation of the slot is incremented at this timing for being reused in the future.
	[[nodiscard]]
	ResourceHandleValidationResult ReleaseSlotAfterResourceDestruction(Handle handle) noexcept
	{
		const auto result = ValidateRegistered(handle);

		if (result != ResourceHandleValidationResult::Success)
		{
			return result;
		}

		ResourceSlotState& slot = m_slots[handle.slotIndex];

		// Slot has to be requested for destruction before it can be released.
		if (!slot.destroyRequested)
		{
			return ResourceHandleValidationResult::DestroyNotRequested;
		}

		// Increment the generation for being reused in the future.
		++slot.generation;
		slot.registered = false;
		slot.destroyRequested = false;

		return ResourceHandleValidationResult::Success;
	}

	// Validates a registered handle against the slot table (no check for destroy request).
	[[nodiscard]]
	ResourceHandleValidationResult ValidateRegistered(Handle handle) const noexcept
	{
		return ValidateRegisteredHandle(handle, m_ownerId, m_slots);
	}

	// Validates a handle for new use (Validation and check for destroy request).
	[[nodiscard]]
	ResourceHandleValidationResult ValidateForNewUse(Handle handle) const noexcept
	{
		return ValidateHandleForNewUse(handle, m_ownerId, m_slots);
	}

	// Return the handle of the slot that is requested for destruction at the given index.
	[[nodiscard]]
	Handle GetDestroyRequestedHandle(std::size_t index) const noexcept
	{
		if (index >= m_slots.size())
		{
			return {};
		}

		const auto& slot = m_slots[index];

		if (!slot.registered || !slot.destroyRequested)
		{
			return {};
		}

		return { m_ownerId, static_cast<uint32_t>(index), slot.generation };
	}

	// Finds the smallest index of the slot that can be reused.
	[[nodiscard]]
	std::size_t FindRegistrationSlotIndex() const noexcept
	{
		std::size_t index = 0;

		while (index < m_slots.size() && !m_slots[index].IsReusable())
		{
			++index;
		}

		return index;
	}

private:
	const uint32_t m_ownerId;
	std::vector<ResourceSlotState> m_slots;
};

// Specific type aliases for resource slot tables
using GeometrySlotTable = ResourceSlotTable<GeometryHandle>;
using TextureSlotTable = ResourceSlotTable<TextureHandle>;

}