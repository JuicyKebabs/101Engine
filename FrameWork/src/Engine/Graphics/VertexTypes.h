#pragma once
#include <d3d12.h>
#include "Engine/Core/Math/Math.h"

// ---------------------------------------------------------------------------
// Vertex input layout for static meshes
// ---------------------------------------------------------------------------
struct Vertex
{
	Vector3 position;	// Vertex position
	Vector3 normal;		// Vertex normal
	Vector2 uv;			// Vertex UV coordinates
	Vector3 tangent;	// Vertex tangent
	Vector4 color;		// Vertex color

	static const D3D12_INPUT_LAYOUT_DESC InputLayout;

private:
	static const size_t InputLayoutCount = 5;
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputLayoutCount];
};

// ---------------------------------------------------------------------------
// Vertex input layout for skinned meshes (used by skeletal animation)
// Uncomment and expand when animation system is implemented
// ---------------------------------------------------------------------------
//struct SkinnedVertex
//{
//	Vector3 position;
//	Vector3 normal;
//	Vector2 uv;
//	Vector3 tangent;
//	Vector4 color;
//	DirectX::XMINT4  boneIndices;	// Up to 4 bone influences
//	Vector4          boneWeights;	// Corresponding weights (must sum to 1)
//
//	static const D3D12_INPUT_LAYOUT_DESC InputLayout;
//
//private:
//	static const size_t InputLayoutCount = 7;
//	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputLayoutCount];
//};
