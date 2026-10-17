#pragma once

#include <Windows.h>
#include <cstdint>

//--------------------------------------------------------------------------------
// Win32WindowOutputDesc
// Struct to recieve the window information from the user application.
// Renderer create the swapchain based on the information provided in this struct.
//--------------------------------------------------------------------------------

namespace rendering
{

struct Win32WindowOutputDesc
{
	HWND window;
	uint32_t width;
	uint32_t height;
};

}