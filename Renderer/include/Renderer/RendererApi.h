#pragma once

// Decide the definition of RENDERER_API macro based on
// whether we are building the Renderer DLL or using it.

// RENDERER_BUILD_DLL is defined in the CMakeLists.txt of Redererer project 
// for Private access not to be used by other projects which use the Renderer library.

#if defined(RENDERER_BUILD_DLL)
// For the Renderer library itself, we export the symbols.
	#define RENDERER_API __declspec(dllexport)
#else
// For projects that use the Renderer library, we import the symbols.
	#define RENDERER_API __declspec(dllimport)
#endif