// R7 implementation guide: section 4.8.
// Build color, CPU descriptor heaps and Depth privately, then publish the complete set.
// Local RAII owners clean up an incomplete candidate; no partial output is exposed.
// Creation allocates storage but does not make its pixels initialized or shader-readable.

#include "RenderTargetTextureGpuData.h"
#include "ApiResult.h"
#include "Failure.h"

namespace rendering::backend
{

// Every local owner below is provisional until the final commit.
// Returning early destroys the candidate color/heaps/Depth without exposing any partial public output.
Result RenderTargetTextureGpuData::Initialize(
	ID3D12Device* device,
	const RenderTargetTextureDesc& description)
{
	if (!device || m_color)
	{
		return detail::Fail("RenderTargetTexture: device and empty destination are required.");
	}

	if (description.width == 0 || description.height == 0)
	{
		return detail::Fail("RenderTargetTexture: dimensions must be non-zero.");
	}

	const bool extentSupported = description.width <= D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION &&
		description.height <= D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	if (!extentSupported)
	{
		return detail::Fail("RenderTargetTexture: dimensions exceed the supported 2D limit.");
	}

	if (description.format != TextureFormat::RGBA8_UNorm)
	{
		return detail::Fail("RenderTargetTexture: R7 supports RGBA8_UNorm only.");
	}

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC colorDescription{};
	colorDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	colorDescription.Width = description.width;
	colorDescription.Height = description.height;
	colorDescription.DepthOrArraySize = 1;
	colorDescription.MipLevels = 1;
	colorDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	colorDescription.SampleDesc.Count = 1;
	colorDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	// This is an allocation hint, not a GPU Clear or initialization operation.
	// This tells the driver the common Clear value for efficient allocation/layout decisions.
	// It does not execute a Clear; contentsValid must remain false after allocation.
	// The actual initialization is a later Graph attachment Clear on the GPU.
	D3D12_CLEAR_VALUE optimizedClear{};
	optimizedClear.Format = colorDescription.Format;
	optimizedClear.Color[0] = 0.05f;
	optimizedClear.Color[1] = 0.10f;
	optimizedClear.Color[2] = 0.25f;
	optimizedClear.Color[3] = 1.0f;

	Microsoft::WRL::ComPtr<ID3D12Resource> color;
	HRESULT created = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&colorDescription,
		D3D12_RESOURCE_STATE_COMMON,
		&optimizedClear,
		IID_PPV_ARGS(color.GetAddressOf()));
	if (FAILED(created))
	{
		return MakeApiFailure("CreateCommittedResource(RenderTargetTexture)", created);
	}

	auto rtvHeap = std::make_unique<DescriptorHeap>();
	Result prepared = rtvHeap->Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
	if (!prepared.IsSuccess())
	{
		return prepared;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
	prepared = rtvHeap->GetCpuHandle(0, rtv);
	if (!prepared.IsSuccess())
	{
		return prepared;
	}

	D3D12_RENDER_TARGET_VIEW_DESC renderTargetView{};
	renderTargetView.Format = colorDescription.Format;
	renderTargetView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	device->CreateRenderTargetView(color.Get(), &renderTargetView, rtv);

	// This is CPU descriptor storage, not the shader-visible frame heap.
	// Sampling copies the SRV into FrameResources later; copying a descriptor does not copy the pixels.
	auto srvHeap = std::make_unique<DescriptorHeap>();
	prepared = srvHeap->Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
	if (!prepared.IsSuccess())
	{
		return prepared;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE srv{};
	prepared = srvHeap->GetCpuHandle(0, srv);
	if (!prepared.IsSuccess())
	{
		return prepared;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceView{};
	shaderResourceView.Format = colorDescription.Format;
	shaderResourceView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	shaderResourceView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shaderResourceView.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(color.Get(), &shaderResourceView, srv);

	const HRESULT descriptorStatus = device->GetDeviceRemovedReason();
	if (FAILED(descriptorStatus))
	{
		return MakeApiFailure("CreateViews(RenderTargetTexture): device removed", descriptorStatus);
	}

	// Depth has the same extent and belongs to this output revision.
	// Even equal-sized outputs cannot share depth values because their Cameras and Objects may differ.
	auto depth = std::make_unique<DepthTexture>();
	prepared = depth->Ensure(device, description.width, description.height);
	if (!prepared.IsSuccess())
	{
		return prepared;
	}

	// Publish only the complete set. Clear/Draw, not allocation, makes contents valid.
	m_color = std::move(color);
	m_rtvHeap = std::move(rtvHeap);
	m_srvHeap = std::move(srvHeap);
	m_depth = std::move(depth);
	m_rtv = rtv;
	m_srv = srv;
	width = description.width;
	height = description.height;
	return {};
}

}
