#include "Engine/Resource/GpuTexture.h"

void GpuTexture::Initialize(ID3D12Device* pDevice, DescriptorHeapAllocator* allocator, const GpuTexture::InitDesc& desc)
{
	assert(!(desc.useDSV && desc.useUAV) &&"GpuTexture: DSV and UAV cannot be used together");
	assert(!(desc.initialState == ResourceState::RenderTarget && !desc.useRTV) && "GpuTexture: InitialState::RenderTarget requires useRTV=true");
	assert(!(desc.initialState == ResourceState::DepthWrite && !desc.useDSV) && "GpuTexture: InitialState::DepthWrite requires useDSV=true");
	assert(!(desc.initialState == ResourceState::UnorderedAccess && !desc.useUAV) && "GpuTexture: InitialState::UnorderedAccess requires useUAV=true");

	// Set parameters from the initialization description
	m_width = desc.width;
	m_height = desc.height;
	m_currentState = desc.initialState;
	m_clearColor[0] = desc.clearColor[0];
	m_clearColor[1] = desc.clearColor[1];
	m_clearColor[2] = desc.clearColor[2];
	m_clearColor[3] = desc.clearColor[3];
	m_depthFormat = desc.depthFormat;
	m_clearDepth = desc.clearDepth;

	// Determine the resource format based on the usage
	DXGI_FORMAT resourceFormat;
	if (desc.useDSV && desc.useSRV)
	{// In case of using both DSV and SRV
		if (m_depthFormat == DepthFormat::D16_UNORM) { resourceFormat = DXGI_FORMAT_R16_TYPELESS; m_colorFormat = GpuTexture::ColorFormat::R16_UNORM; }
		else if (m_depthFormat == DepthFormat::D24F) { resourceFormat = DXGI_FORMAT_R24G8_TYPELESS; m_colorFormat = GpuTexture::ColorFormat::R24_UNORM_X8_TYPELESS; }
		else if (m_depthFormat == DepthFormat::D32F) { resourceFormat = DXGI_FORMAT_R32_TYPELESS; m_colorFormat = GpuTexture::ColorFormat::R32F; }
		else { assert(false && "GpuTexture: Invalid depth format for DSV+SRV"); resourceFormat = DXGI_FORMAT_UNKNOWN; }
	}
	else if(desc.useDSV)
	{// If only DSV is used, use the depth format directly
		resourceFormat = ConvertToDXGIDepthFormat(m_depthFormat);
	}
	else 
	{// If DSV is not used, use the color format
		m_colorFormat = desc.format;
		resourceFormat = ConvertToDXGIColorFormat(m_colorFormat);
	}

	// Convert the initial state to D3D12 resource state
	D3D12_RESOURCE_STATES initialState = ConvertToD3D12State(desc.initialState);

	// Set up clear value( only for RTV or DSV)
	D3D12_CLEAR_VALUE clearValue = {};
	D3D12_CLEAR_VALUE* pClearValue = nullptr;
	if (desc.useRTV)
	{
		clearValue = CD3DX12_CLEAR_VALUE(resourceFormat, m_clearColor);
		pClearValue = &clearValue;
	}
	else if (desc.useDSV)
	{
		clearValue.Format = ConvertToDXGIDepthFormat(m_depthFormat);
		clearValue.DepthStencil.Depth = m_clearDepth;
		clearValue.DepthStencil.Stencil = 0;
		pClearValue = &clearValue;
	}

	// Set up heap properties and resource description
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		resourceFormat,
		desc.width, desc.height,
		1,1
	);
	if (desc.useRTV) { resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; }
	if (desc.useDSV) { resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; }
	if (desc.useUAV) { resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; }

	// Create resource
	HRESULT result = pDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		pClearValue,
		IID_PPV_ARGS(m_pResource.ReleaseAndGetAddressOf())
	);
	assert(SUCCEEDED(result) && "GpuTexture: Failed to create resource");

	// Create views if needed
	if (desc.useRTV)
	{// Create RTV
		m_rtvIndex = allocator->AllocateRtv();
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = allocator->GetRtvCpuHandle(m_rtvIndex);
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = ConvertToDXGIColorFormat(m_colorFormat);
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		pDevice->CreateRenderTargetView(
			m_pResource.Get(),
			&rtvDesc,
			rtvHandle
		);
	}
	if (desc.useDSV)
	{// Create DSV
		m_dsvIndex = allocator->AllocateDsv();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = allocator->GetDsvCpuHandle(m_dsvIndex);
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = ConvertToDXGIDepthFormat(m_depthFormat);
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		pDevice->CreateDepthStencilView(
			m_pResource.Get(),
			&dsvDesc,
			dsvHandle
		);
	}
	if (desc.useSRV)
	{// Create SRV
		m_srvIndex = allocator->AllocateCbvSrvUav();
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = allocator->GetCbvSrvUavCpuHandle(m_srvIndex);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = ConvertToDXGIColorFormat(m_colorFormat);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		pDevice->CreateShaderResourceView(
			m_pResource.Get(),
			&srvDesc,
			srvHandle
		);
	}
	if (desc.useUAV)
	{// Create UAV
		m_uavIndex = allocator->AllocateCbvSrvUav();
		D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = allocator->GetCbvSrvUavCpuHandle(m_uavIndex);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = ConvertToDXGIColorFormat(m_colorFormat);
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		pDevice->CreateUnorderedAccessView(
			m_pResource.Get(),
			nullptr,
			&uavDesc,
			uavHandle
		);
	}
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
