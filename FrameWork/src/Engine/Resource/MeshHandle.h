#pragma once
#include "Renderer/ResourceHandle.h"

// Engine code may retain this familiar type name, but identities are now checked
// by the independent Renderer owner/slot/generation registry.
using MeshHandle = rendering::GeometryHandle;
inline constexpr MeshHandle InvalidMeshHandle;
