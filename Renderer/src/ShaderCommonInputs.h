#pragma once

#include "Renderer/FrameRenderData.h"

#include <cstddef>
#include <type_traits>

namespace rendering::detail
{

// Shared CPU/HLSL ABI. Reflection and command preparation use these same layouts.
struct ViewConstants
{
	Matrix4x4 view;
	Matrix4x4 projection;
	Vector4 cameraPosition;
	Matrix4x4 shadowViewProjection;
	Vector4 shadowInformation;
};

struct ObjectConstants
{
	Matrix4x4 world;
	Matrix4x4 normalMatrix;
	Vector4 tint;
};

static_assert(std::is_standard_layout_v<ViewConstants>);
static_assert(std::is_standard_layout_v<ObjectConstants>);
static_assert(sizeof(ViewConstants) == 224);
static_assert(offsetof(ViewConstants, shadowViewProjection) == 144);
static_assert(offsetof(ViewConstants, shadowInformation) == 208);
static_assert(sizeof(ObjectConstants) == 144);
static_assert(offsetof(ObjectConstants, normalMatrix) == 64);
static_assert(offsetof(ObjectConstants, tint) == 128);
static_assert(sizeof(DirectionalLight) == 48);

} // namespace rendering::detail
