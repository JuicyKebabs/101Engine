#pragma once
#include "Engine/Core/Math/Math.h"

// ---------------------------------------------------------------------------
// CPU-side directional light description.
// This is the editable light state held by SceneBase and passed to Renderer.
// It is distinct from LightConstants (the GPU-side constant buffer layout).
// ---------------------------------------------------------------------------
struct DirectionalLight
{
	Vector3   position  = { 0.0f,  0.0f, 0.0f };	// Shadow-map light origin
	Vector3   direction = { -1.0f, -1.0f, 0.0f };	// World-space direction (unnormalized OK; normalized in shader)
	Matrix4x4 view;									// Light view matrix (rebuilt each frame for shadow)
	Matrix4x4 proj;									// Light projection matrix
	float     intensity = 1.0f;
	Vector3   color     = { 1.0f, 1.0f, 1.0f };
	float     ambient   = 0.1f;
	bool      enabled   = true;
};
