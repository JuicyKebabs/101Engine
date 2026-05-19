#pragma once
#include "Engine/Core/Math/Math.h"

// ---------------------------------------------------------------------------
// GPU Constant Buffer layouts
// These structs are mapped directly to HLSL constant buffers.
// alignas(256) is required by D3D12 (minimum CB size / alignment = 256 bytes).
//
// Register assignments:
//   b0 - FrameConstants        (per-frame / camera)
//   b1 - *RenderConstants      (per-object, varies by pass type)
//   b2 - LightConstants        (per-scene lighting)
// ---------------------------------------------------------------------------

// b0 - Updated once per frame; shared across all draw calls
struct alignas(256) FrameConstants
{
	Matrix4x4 view;				// View matrix
	Matrix4x4 proj;				// Projection matrix
	Vector3   cameraPosition;	// World-space camera position (for lighting)
};

// b1 - Per-object data for static mesh rendering
struct alignas(256) MeshRenderConstants
{
	Matrix4x4 worldMatrix;			// Object-to-world transform
	Matrix4x4 worldInvTranspose;	// Inverse transpose of world (for correct normal transform)
	Matrix4x4 lightViewProj;		// Light-space VP matrix (for shadow map lookup)
	Vector4   objectColor;			// Tint color (RGBA)
};

// b1 - Per-object data for sprite rendering
struct alignas(256) SpriteRenderConstants
{
	Matrix4x4 worldMatrix;	// Object-to-world transform
	Vector4   color;		// Tint color (RGBA)
	Vector4   uvRect;		// UV sub-rectangle: (u, v, scaleU, scaleV)
	Vector2   pivot;		// Pivot point in [0,1] space
	Vector2   flip;			// Axis flip: (1,-1) to flip; (1,1) for none
};

// b1 - Per-object data for UI rendering
struct alignas(256) UIRenderConstants
{
	Matrix4x4 worldMatrix;	// Object-to-world transform
	Vector4   color;		// Tint color (RGBA)
	Vector4   uvRect;		// UV sub-rectangle: (u, v, scaleU, scaleV)
	Vector2   flip;			// Axis flip flags
};

// b2 - Scene-level directional light data for shading
struct alignas(256) LightConstants
{
	Vector4 lightDir_Intensity;		// xyz = light direction (world space), w = intensity
	Vector4 lightColor_Ambient;		// xyz = light color, w = ambient intensity
};
