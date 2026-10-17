#pragma once

#include <cstdint>
#include <limits>

//----------------------------------------------------------------------------------------------------
// Resource state management
// Each resources are managed with a state to track their preparation, destruction, and usage states.
//----------------------------------------------------------------------------------------------------

namespace rendering::detail
{

enum class ResourcePreparationState
{
	Pending = 0,	// The resource is pending preparation.
	Ready = 1,		// The resource is ready for use.
	Failed = 2,		// The resource preparation has failed.
};

// State of one particular resource content
struct ResourceContentState
{
	ResourcePreparationState preparationState = ResourcePreparationState::Pending;
};

// State of one particular resource slot (Slot has this state even while it is empty)
struct ResourceSlotState
{
	static constexpr uint32_t ExhaustedGeneration = (std::numeric_limits<uint32_t>::max)();

	uint32_t generation = 0;
	bool registered = false;
	bool destroyRequested = false;

	[[nodiscard]]
	bool IsReusable() const noexcept
	{
		return !registered && !destroyRequested && generation != ExhaustedGeneration;
	}
};

}