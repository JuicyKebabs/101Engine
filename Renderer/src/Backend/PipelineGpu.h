#pragma once

#include "Renderer/Result.h"
#include "Renderer/DrawTypes.h"
#include "Graph/OutputLayout.h"
#include "Resource/ShaderData.h"

#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

//----------------------------------------------------------------------------------------------
// PipelineGpu.h
// Backend utility functions for creating root signatures and graphics pipelines in DirectX 12.
//----------------------------------------------------------------------------------------------

namespace rendering::backend
{

struct VertexField
{
	const char* semantic;
	UINT count;
	UINT offset;
	DXGI_FORMAT format;
};

std::vector<VertexField> VertexFields();

struct PipelineGpuData
{
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
};

struct PipelineBuildInputs
{
	const PipelineDesc* description = nullptr;
	const ShaderMetadata* layout = nullptr;
	const detail::ShaderContent* vertex = nullptr;
	const detail::ShaderContent* pixel = nullptr;
	const graph::OutputLayout* output = nullptr;
	DepthMode depth = DepthMode::Disabled;
};

Result BuildPipeline(ID3D12Device* device, const PipelineBuildInputs& inputs, PipelineGpuData& output);

Result CreateRootSignature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& desc,
                           Microsoft::WRL::ComPtr<ID3D12RootSignature>& output);

Result CreateGraphicsPipeline(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                              Microsoft::WRL::ComPtr<ID3D12PipelineState>& output);

} // namespace rendering::backend