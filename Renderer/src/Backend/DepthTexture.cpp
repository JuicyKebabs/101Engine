#include "DepthTexture.h"
#include "ApiResult.h"

namespace rendering::backend
{

Result DepthTexture::Ensure(ID3D12Device* device, UINT width, UINT height, bool allowSampling)
{
	if (!device)
	{
		return { false, "DepthTexture: device is null." };
	}

	if (width == 0 || height == 0)
	{
		return { false, "DepthTexture: width and height must be non-zero." };
	}

	if (m_resource)
	{
		auto desc = m_resource->GetDesc();

		if (desc.Width != width || desc.Height != height || allowSampling != AllowsSampling())
		{
			return { false, "DepthTexture: existing resource dimensions do not match requested dimensions." };
		}

		return {};
	}

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = allowSampling ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if (!allowSampling)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	auto hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&resource));

	if (FAILED(hr))
	{
		return MakeApiFailure("DepthTexture: CreateCommittedResource", hr);
	}

	Result result = m_dsvHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	if (!result.IsSuccess())
	{
		return result;
	}

	result = m_dsvHeap.GetCpuHandle(0, m_dsvHandle);

	if (!result.IsSuccess())
	{
		return result;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
	viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		
	device->CreateDepthStencilView(resource.Get(), &viewDesc, m_dsvHandle);
	hr = device->GetDeviceRemovedReason();

	if (FAILED(hr))
	{
		return MakeApiFailure("DepthTexture: CreateDepthStencilView", hr);
	}

    if (allowSampling)
    {
        // The same typeless storage has a typed DSV and a typed scalar SRV.
        // State transitions and Graph dependency validation remain mandatory.
        result = m_srvHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
        if (!result.IsSuccess())
        {
            return result;
        }

        result = m_srvHeap.GetCpuHandle(0, m_srvHandle);
        if (!result.IsSuccess())
        {
            return result;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC sampleDescription;
        sampleDescription = {};
        sampleDescription.Format = DXGI_FORMAT_R32_FLOAT;
        sampleDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sampleDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sampleDescription.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(resource.Get(), &sampleDescription, m_srvHandle);
        const HRESULT sampleStatus = device->GetDeviceRemovedReason();
        if (FAILED(sampleStatus))
        {
            return MakeApiFailure("DepthTexture: CreateShaderResourceView", sampleStatus);
        }
    }

	m_resource = std::move(resource);
	return {};
}

}