#pragma once

#include "Renderer/ResourceHandle.h"
#include "ResourceState.h"

#include <cstdint>
#include <span>
#include <string>

//--------------------------------------------------------------------------------------------------------
// Resource handle validation
// Validating resource handles and returns the result of the validation.
// State of being requested to destroy is not considered invalid for the using resource via the handle,
// but it is considered invalid for the new use of the resource via the handle (It have to be destroyed).
//--------------------------------------------------------------------------------------------------------

namespace rendering::detail
{

enum class ResourceHandleValidationResult
{
	Success,
	InvalidHandle,
	OwnerMismatch,
	SlotOutOfRange,
	SlotNotRegistered,
	GenerationMismatch,
	DestroyRequested,
	DestroyNotRequested
};

// A diagnostic describes the rejected public identity; substitution never
// rewrites that identity into a valid registration or hides its failure cause.
inline const char* ResourceValidationName(ResourceHandleValidationResult result) noexcept
{
    switch (result)
    {
    case ResourceHandleValidationResult::Success: return "identity valid; Content unavailable";
    case ResourceHandleValidationResult::InvalidHandle: return "initial Invalid handle";
    case ResourceHandleValidationResult::OwnerMismatch: return "different Renderer owner";
    case ResourceHandleValidationResult::SlotOutOfRange: return "slot out of range";
    case ResourceHandleValidationResult::SlotNotRegistered: return "registration released";
    case ResourceHandleValidationResult::GenerationMismatch: return "stale generation";
    case ResourceHandleValidationResult::DestroyRequested: return "Destroy requested";
    case ResourceHandleValidationResult::DestroyNotRequested: return "Destroy not requested";
    }

    return "unknown identity state";
}

template<class Handle>
std::string DescribeResourceIdentity(Handle handle, ResourceHandleValidationResult result)
{
    return std::string(ResourceValidationName(result)) + " (owner=" + std::to_string(handle.ownerId)
        + ", slot=" + std::to_string(handle.slotIndex) + ", generation=" + std::to_string(handle.generation) + ")";
}

template<class Tag>
[[nodiscard]]
ResourceHandleValidationResult ValidateRegisteredHandle(
	const ResourceHandle<Tag>& handle,
	uint32_t currentOwnerId,
	std::span<const ResourceSlotState> slots) noexcept
{
	constexpr auto invalid = ResourceHandle<Tag>::InvalidIndex;

	// Owning invalid value 
	if (handle.ownerId == invalid ||
		handle.slotIndex == invalid ||
		handle.generation == invalid)
	{
		return ResourceHandleValidationResult::InvalidHandle;
	}

	// Owned by a different renderer instance
	if (currentOwnerId == invalid || handle.ownerId != currentOwnerId)
	{
		return ResourceHandleValidationResult::OwnerMismatch;
	}

	// Handle is assigned to a slot which is out of range of the slots array
	if (handle.slotIndex >= slots.size())
	{
		return ResourceHandleValidationResult::SlotOutOfRange;
	}

	const ResourceSlotState& slot = slots[handle.slotIndex];

	// Handle is indicating a slot that is not registered
	if (!slot.registered)
	{
		return ResourceHandleValidationResult::SlotNotRegistered;
	}

	// Handle is indicating a slot that has a defferet generation
	if (handle.generation != slot.generation)
	{
		return ResourceHandleValidationResult::GenerationMismatch;
	}

	return ResourceHandleValidationResult::Success;
}

template<class Tag>
[[nodiscard]]
ResourceHandleValidationResult ValidateHandleForNewUse(
	const ResourceHandle<Tag>& handle,
	std::uint32_t currentOwnerId,
	std::span<const ResourceSlotState> slots) noexcept
{
	const auto result = ValidateRegisteredHandle(handle, currentOwnerId, slots);

	if (result != ResourceHandleValidationResult::Success)
	{
		return result;
	}

	// Handle is indicating a slot that has a destroy request pending
	if (slots[handle.slotIndex].destroyRequested)
	{
		return ResourceHandleValidationResult::DestroyRequested;
	}

	return ResourceHandleValidationResult::Success;
}

}