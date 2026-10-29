#pragma once

namespace rendering
{

// Struct to hold configuration options for initializing the Renderer,
struct RendererConfiguration
{
	bool enableDebugLayer = false;
	bool enableGpuValidation = false;
};

}