#pragma once

#include "GraphTextureBinding.h"
#include "Backend/Backend.h"

//-------------------------------------------------------------------------------------------
// FrameTextureResolver
// Helper to manage the state of texture resources between the render graph and the backend.
//-------------------------------------------------------------------------------------------

namespace rendering::graph
{

// Query a texture binding from the backend.
// Borrowed binding is managing the state of resource individually, 
// and it is not guaranteed to be the same as the backend's current state.
Result QueryBinding(
    backend::Backend& backend,
    const GraphTextureBinding& borrowed,
    GraphTextureBinding& output);

// Commit the final state of a texture binding after graph execution to the backend.
void CommitBinding(backend::Backend& backend,
    const GraphTextureBinding& binding,
    D3D12_RESOURCE_STATES state, bool contentsValid) noexcept;
}