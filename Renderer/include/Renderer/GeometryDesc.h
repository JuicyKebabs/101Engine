#pragma once

#include "Renderer/Math.h"

#include <cstdint>
#include <span>

//------------------------------------------------------------------------
// Geometry description
// Used to register geometry resources via Register() API from user code.
//------------------------------------------------------------------------

namespace rendering
{

enum class PrimitiveTopology
{
	TriangleList,
};

struct MeshVertex
{
	Vector3 position{};
	Vector3 normal{};
	Vector2 uv{};
	Vector3 tangent{};
	Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct GeometryDesc
{
	std::span<const MeshVertex> vertices;
	std::span<const uint32_t> indices;
	PrimitiveTopology topology = PrimitiveTopology::TriangleList;
};

}
