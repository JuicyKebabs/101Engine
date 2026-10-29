#pragma once
#include "Engine/Core/Math/Math.h"

// Legacy CPU generator layout; converted by value at the Renderer boundary.
// Framework no longer publishes an old native InputLayout or GPU VertexBuffer.
struct Vertex
{
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    Vector3 tangent;
    Vector4 color;
};
