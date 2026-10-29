#include "PipelineGpu.h"
#include "ApiResult.h"

namespace rendering::backend
{

Result CreateRootSignature(
    ID3D12Device* device,
    const D3D12_ROOT_SIGNATURE_DESC& desc,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>& output)
{
	if (!device)
	{
		return { false, "CreateRootSignature: device is null." };
	}

	if (output)
	{
		return { false, "CreateRootSignature: output already contains a root signature." };
	}

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	auto hr = D3D12SerializeRootSignature(
		&desc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr))
	{
		std::string errorMessage = "CreateRootSignature: D3D12SerializeRootSignature failed.";

		if (errorBlob)
		{
			errorMessage += " Error: " + std::string(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
		}

		return MakeApiFailure(errorMessage.c_str(), hr);
	}

	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&output)
	);

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateRootSignature: ID3D12Device::CreateRootSignature failed.", hr);
	}

	return {};
}

Result CreateGraphicsPipeline(
	ID3D12Device* device,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
	Microsoft::WRL::ComPtr<ID3D12PipelineState>& output)
{
	if (!device)
	{
		return { false, "CreateGraphicsPipeline: device is null." };
	}

	if (output)
	{
		return { false, "CreateGraphicsPipeline: output already contains a pipeline state." };
	}

	auto hr = device->CreateGraphicsPipelineState(
		&desc,
		IID_PPV_ARGS(&output)
	);

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateGraphicsPipeline: ID3D12Device::CreateGraphicsPipelineState failed.", hr);
	}

	return {};
}

}