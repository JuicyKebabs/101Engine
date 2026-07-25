#include "Engine/Resource/GpuTexture.h"

void GpuTexture::Initialize(ID3D12Device* pDevice, DescriptorHeapAllocator* allocator, const GpuTexture::ParamDesc& desc)
{
	assert(pDevice);
	assert(allocator);
	assert(!m_isInitialized);
	assert(desc.width > 0 && desc.height > 0);
	assert(!(desc.useDSV && desc.useUAV) &&"GpuTexture: DSV and UAV cannot be used together");
	assert(!(desc.initialState == ResourceState::RenderTarget && !desc.useRTV) && "GpuTexture: InitialState::RenderTarget requires useRTV=true");
	assert(!(desc.initialState == ResourceState::DepthWrite && !desc.useDSV) && "GpuTexture: InitialState::DepthWrite requires useDSV=true");
	assert(!(desc.initialState == ResourceState::UnorderedAccess && !desc.useUAV) && "GpuTexture: InitialState::UnorderedAccess requires useUAV=true");

	// Create the GPU resource based on the provided description
	ComPtr<ID3D12Resource> newResource;
	ColorFormat resolvedColorFormat = ColorFormat::UNKNOWN;
	const bool created = CreateResource(
		pDevice,
		desc,
		newResource,
		resolvedColorFormat
	);

	assert(created);

	if (!created) return;

	// Apply the description to the GpuTexture instance
	m_desc = desc;
	ApplyDesc(m_desc);

	m_colorFormat = resolvedColorFormat;
	m_pResource = std::move(newResource);

	// Allocate descriptors for RTV, DSV, SRV, and UAV as needed
	AllocateDescriptors(allocator);

	// Create required views (RTV, DSV, SRV, UAV) for the GPU resource
	CreateViews(pDevice, allocator);

	m_isInitialized = true;
}

void GpuTexture::TransitionToState(ID3D12GraphicsCommandList* cmdList, ResourceState newState)
{
	if (m_currentState == newState) return;
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_pResource.Get(),						// Current render target resource
			ConvertToD3D12State(m_currentState),	// Current resource state
			ConvertToD3D12State(newState)			// New resource state for rendering
		);
	cmdList->ResourceBarrier(1, &barrier);
	m_currentState = newState;
}

void GpuTexture::ApplyDesc(const ParamDesc& desc)
{
	m_width = desc.width;
	m_height = desc.height;
	m_currentState = desc.initialState;

	m_clearColor[0] = desc.clearColor[0];
	m_clearColor[1] = desc.clearColor[1];
	m_clearColor[2] = desc.clearColor[2];
	m_clearColor[3] = desc.clearColor[3];

	m_depthFormat = desc.depthFormat;
	m_clearDepth = desc.clearDepth;
}

bool GpuTexture::CreateResource(
	ID3D12Device* device,
	const ParamDesc& desc,
	ComPtr<ID3D12Resource>& outResource,
	ColorFormat& outColorFormat) const
{
	if (!device || desc.width == 0 || desc.height == 0) return false;

	//------------------------------
	// 1. Resource Format Selection
	//------------------------------

	DXGI_FORMAT resourceFormat = DXGI_FORMAT_UNKNOWN;
	outColorFormat = desc.format;

	if (desc.useDSV && desc.useSRV)
	{	// In case of using both DSV and SRV, we need to use a typeless format for the resource
		switch (desc.depthFormat)
		{
		case DepthFormat::D16_UNORM:
			resourceFormat = DXGI_FORMAT_R16_TYPELESS;
			outColorFormat = ColorFormat::R16_UNORM;
			break;

		case DepthFormat::D24F:
			resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
			outColorFormat =
				ColorFormat::R24_UNORM_X8_TYPELESS;
			break;

		case DepthFormat::D32F:
			resourceFormat = DXGI_FORMAT_R32_TYPELESS;
			outColorFormat = ColorFormat::R32F;
			break;

		default:
			return false;
		}
	}
	else if (desc.useDSV)
	{// If only DSV is used, use the depth format directly
		resourceFormat = ConvertToDXGIDepthFormat(desc.depthFormat);
	}
	else
	{// If DSV is not used, use the color format
		resourceFormat = ConvertToDXGIColorFormat(desc.format);
	}

	// DXGIFormat was not set correctly, return false
	if (resourceFormat == DXGI_FORMAT_UNKNOWN) return false;

	//------------------------------
	// 2. Resource State Conversion
	//------------------------------

	const D3D12_RESOURCE_STATES initialState = ConvertToD3D12State(desc.initialState);

	D3D12_CLEAR_VALUE clearValue{};
	D3D12_CLEAR_VALUE* clearValuePointer = nullptr;

	if (desc.useRTV)
	{// In case of using RTV, we need to set the clear value for the render target
		clearValue =CD3DX12_CLEAR_VALUE(resourceFormat, desc.clearColor);
		clearValuePointer = &clearValue;
	}
	else if (desc.useDSV)
	{// In case of using DSV, we need to set the clear value for the depth stencil
		clearValue.Format =ConvertToDXGIDepthFormat(desc.depthFormat);
		clearValue.DepthStencil.Depth = desc.clearDepth;

		clearValue.DepthStencil.Stencil = 0;
		clearValuePointer = &clearValue;
	}

	//---------------------------------------------
	// 3. Heap Properties and Resource Description
	//---------------------------------------------

	const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_RESOURCE_DESC resourceDesc =
		CD3DX12_RESOURCE_DESC::Tex2D(
			resourceFormat,
			desc.width,
			desc.height,
			1,
			1
		);

	// Allow render target usage
	if (desc.useRTV) resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	// Allow depth stencil usage
	if (desc.useDSV) resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// Allow unordered access usage
	if (desc.useUAV) resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	//------------------------
	// 4. Create the Resource
	//------------------------

	ComPtr<ID3D12Resource> newResource;

	const HRESULT result =
		device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			initialState,
			clearValuePointer,
			IID_PPV_ARGS(newResource.GetAddressOf())
		);

	if (FAILED(result)) return false;

	outResource = std::move(newResource);
	return true;
}

void GpuTexture::AllocateDescriptors(DescriptorHeapAllocator* allocator)
{
	assert(allocator);

	if (m_desc.useRTV)
	{
		m_rtvIndex = allocator->AllocateRtv();
		assert(m_rtvIndex != UINT32_MAX);
	}

	if (m_desc.useDSV)
	{
		m_dsvIndex = allocator->AllocateDsv();
		assert(m_dsvIndex != UINT32_MAX);
	}

	if (m_desc.useSRV)
	{
		m_srvIndex = allocator->AllocateCbvSrvUav();
		assert(m_srvIndex != UINT32_MAX);
	}

	if (m_desc.useUAV)
	{
		m_uavIndex = allocator->AllocateCbvSrvUav();
		assert(m_uavIndex != UINT32_MAX);
	}
}

void GpuTexture::CreateViews(
	ID3D12Device* device,
	DescriptorHeapAllocator* allocator
)
{
	assert(device);
	assert(allocator);
	assert(m_pResource);

	// RTV
	if (m_desc.useRTV)
	{
		assert(m_rtvIndex != UINT32_MAX);

		const D3D12_CPU_DESCRIPTOR_HANDLE handle = allocator->GetRtvCpuHandle(m_rtvIndex);

		D3D12_RENDER_TARGET_VIEW_DESC viewDesc{};
		viewDesc.Format = ConvertToDXGIColorFormat(m_colorFormat);
		viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		device->CreateRenderTargetView(
			m_pResource.Get(),
			&viewDesc,
			handle
		);
	}

	// DSV
	if (m_desc.useDSV)
	{
		assert(m_dsvIndex != UINT32_MAX);

		const D3D12_CPU_DESCRIPTOR_HANDLE handle = allocator->GetDsvCpuHandle(m_dsvIndex);

		D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
		viewDesc.Format = ConvertToDXGIDepthFormat(m_depthFormat);
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		device->CreateDepthStencilView(
			m_pResource.Get(),
			&viewDesc,
			handle
		);
	}

	// SRV
	if (m_desc.useSRV)
	{
		assert(m_srvIndex != UINT32_MAX);

		const D3D12_CPU_DESCRIPTOR_HANDLE handle = allocator->GetCbvSrvUavCpuHandle(m_srvIndex);

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = ConvertToDXGIColorFormat(m_colorFormat);
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Texture2D.MipLevels = 1;

		device->CreateShaderResourceView(
			m_pResource.Get(),
			&viewDesc,
			handle
		);
	}

	// UAV
	if (m_desc.useUAV)
	{
		assert(m_uavIndex != UINT32_MAX);

		const D3D12_CPU_DESCRIPTOR_HANDLE handle = allocator->GetCbvSrvUavCpuHandle(m_uavIndex);

		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = ConvertToDXGIColorFormat(m_colorFormat);
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = 0;

		device->CreateUnorderedAccessView(
			m_pResource.Get(),
			nullptr,
			&viewDesc,
			handle
		);
	}
}

bool GpuTexture::Resize(
	ID3D12Device* device,
	DescriptorHeapAllocator* allocator,
	UINT width,
	UINT height
)
{
	// Guard unacceptable parameters
	if (!m_isInitialized ||
		!device			 ||
		!allocator		 ||
		width == 0		 ||
		height == 0)
	{
		return false;
	}

	// Don't resize to the same dimensions
	if (width == m_width &&
		height == m_height)
	{
		return true;
	}

	// Recreate the resource with the new dimensions based on stored parameters
	ParamDesc resizedDesc = m_desc;
	resizedDesc.width = width;
	resizedDesc.height = height;

	ComPtr<ID3D12Resource> newResource;
	ColorFormat resolvedColorFormat = ColorFormat::UNKNOWN;

	if (!CreateResource(device,
		resizedDesc,
		newResource,
		resolvedColorFormat))
	{
		return false;
	}

	// Update the GpuTexture instance with the new resource and dimensions
	m_desc = resizedDesc;
	ApplyDesc(m_desc);

	m_colorFormat = resolvedColorFormat;
	m_pResource = std::move(newResource);

	// Recreate the views for the new resource (use the same descriptor indices as before)
	CreateViews(device, allocator);

	return true;

}