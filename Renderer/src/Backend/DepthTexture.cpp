#include "DepthTexture.h"
#include "ApiResult.h"

namespace rendering::backend
{

Result DepthTexture::Ensure(ID3D12Device* device, UINT width, UINT height)
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

		if (desc.Width != width || desc.Height != height)
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
	resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = resourceDesc.Format;
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
	viewDesc.Format = resourceDesc.Format;
	viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		
	device->CreateDepthStencilView(resource.Get(), &viewDesc, m_dsvHandle);
	hr = device->GetDeviceRemovedReason();

	if (FAILED(hr))
	{
		return MakeApiFailure("DepthTexture: CreateDepthStencilView", hr);
	}

	m_resource = std::move(resource);
	return {};
}

}