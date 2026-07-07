#pragma once
#include <cstdint>

//--------------------------------------
// MeshHandle
// A handle type for identifying meshes
//--------------------------------------

using MeshHandle = uint32_t;
constexpr MeshHandle InvalidMeshHandle = UINT32_MAX;
