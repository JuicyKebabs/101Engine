// R7 implementation guide: section 4.23.
// Validated native draw description, borrowed only while recording the current graphics list.
// VB necessity and indexed/non-indexed mode are independent: ordinary non-indexed Meshes still use VB.
// Every packet contains the effective scissor so one draw cannot inherit another draw's clipping.

#pragma once

#include <d3d12.h>
#include <cstdint>
#include <vector>

//--------------------------------------------
// DrawPacket
// Backend expression for a single draw call.
//--------------------------------------------

namespace rendering::backend
{

enum class DrawKind
{
	Vertices,
	Indexed
};

struct ConstantBinding
{
	UINT rootIndex = 0;
	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress{};
};

struct TextureBinding
{
	UINT rootIndex = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE descriptor{};
};

struct DrawPacket
{
	ID3D12PipelineState* pipeline = nullptr;
	ID3D12RootSignature* root = nullptr;
	ID3D12DescriptorHeap* heap = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertices{};
	D3D12_INDEX_BUFFER_VIEW indices{};
	DrawKind kind = DrawKind::Vertices;
	UINT count = 0;	// Number of vertices or indices to draw.
	UINT first = 0;
	INT baseVertex = 0;
	bool requiresVertexBuffer = true;
	D3D12_RECT scissor{};
	std::vector<ConstantBinding> constants;
	std::vector<TextureBinding> textures;
};

}