#pragma once

#include "Result.h"

#include <cstdint>

//-----------------------------------------------------------------------------------------
// WindowOutput.h
// This file defines the structures related to window output handles and creation results.
// ----------------------------------------------------------------------------------------

namespace rendering
{

// Indicates the handle to the connected window output in renderer backend.
// Renderer creates the swapchain for the Win32 window which the user provides.
// This handle connects that window and the swapchain which is created for that window in the renderer backend.
struct WindowOutputHandle
{
	std::uint64_t value = 0;
};

// This struct is used for the return value of the Renderer::CreateWindowOutput function
// which creates the swapchain and identifier for the given Win32 window output description.
// So, this indicates if that operation is successful or not, and if successful, the handle to the connected window output.
struct [[nodiscard]] WindowOutputCreationResult
{
	Result status;
	WindowOutputHandle handle;
};

}