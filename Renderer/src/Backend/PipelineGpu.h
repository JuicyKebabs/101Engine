#pragma once

#include "Renderer/Result.h"

#include <d3d12.h>
#include <wrl/client.h>

//----------------------------------------------------------------------------------------------
// PipelineGpu.h
// Backend utility functions for creating root signatures and graphics pipelines in DirectX 12.
//----------------------------------------------------------------------------------------------

namespace rendering::backend
{

Result CreateRootSignature(
    ID3D12Device* device,
    const D3D12_ROOT_SIGNATURE_DESC& desc,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>& output);

Result CreateGraphicsPipeline(
    ID3D12Device* device,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
    Microsoft::WRL::ComPtr<ID3D12PipelineState>& output);

}