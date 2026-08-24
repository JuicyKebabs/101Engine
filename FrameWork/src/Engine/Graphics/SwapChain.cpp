#include "SwapChain.h"
#include "Engine/Graphics/DescriptorHeapAllocator.h"
#include "Engine/Core/Debug/Debug.h"

#include <cassert>

bool SwapChain::Initialize(
	ID3D12Device* pDevice,
	ID3D12CommandQueue* pCommandQueue,
	DescriptorHeapAllocator* pDescriptorHeapAllocator,
	HWND hwnd, 
	UINT width, UINT height
)
{
	m_device = pDevice;
	m_descriptorHeapAllocator = pDescriptorHeapAllocator;

	CreateSwapChain(pCommandQueue, hwnd, width, height);
	CreateBackBuffers();
	return true;
}

HRESULT SwapChain::Present(UINT syncInterval, UINT flags)
{
	if (!m_pSwapChain) return E_POINTER;

	return m_pSwapChain->Present(syncInterval, flags);
}

UINT SwapChain::GetCurrentBackBufferIndex() const
{
	assert(m_pSwapChain);
	return m_pSwapChain->GetCurrentBackBufferIndex();
}

bool SwapChain::CreateSwapChain(ID3D12CommandQueue* pCommandQueue, HWND hwnd, UINT width, UINT height)
{
	if (!pCommandQueue || !hwnd)
	{
		DBG("SwapChain::Initialize failed: Invalid command queue or window handle.");
		return false;
	}

	if (width == 0 || height == 0)
	{
		DBG("SwapChain::Initialize failed: Invalid width or height.");
		return false;
	}

	HRESULT result;

	// Create factory
	ComPtr<IDXGIFactory4> factory;
	result = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	if (FAILED(result))
	{
		DBG("SwapChain::Initialize failed: CreateDXGIFactory1 failed with HRESULT 0x%08X.", result);
		return false;
	}

	// Describe the swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = width;									// Width of the swap chain
	swapChainDesc.Height = height;									// Height of the swap chain
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;				// Color format
	swapChainDesc.Stereo = false;									// Not stereo (3D)
	swapChainDesc.SampleDesc.Count = 1;								// No multi-sampling
	swapChainDesc.SampleDesc.Quality = 0;							// Quality level 0
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// Use as render target
	swapChainDesc.BufferCount = BufferCount;						// Number of buffers (double buffering)
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;					// Stretch to fit window size
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;		// Swap effect (flip and discard)
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;			// Alpha mode (unspecified)
	swapChainDesc.Flags = 0;										// Flags (none)

	// Create the swap chain
	ComPtr<IDXGISwapChain1> swapChain1;
	result = factory->CreateSwapChainForHwnd(
		pCommandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1
	);

	if (FAILED(result))
	{
		DBG("SwapChain::Initialize failed: CreateSwapChainForHwnd failed with HRESULT 0x%08X.", result);
		return false;
	}

	// Convert to IDXGISwapChain4
	result = swapChain1->QueryInterface(IID_PPV_ARGS(m_pSwapChain.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		DBG("SwapChain::Initialize failed: Failed to convert to IDXGISwapChain4 with HRESULT 0x%08X.", result);
		return false;
	}

	return true;
}

bool SwapChain::CreateBackBuffers()
{
	if (!m_device)
	{
		DBG("SwapChain::CreateBackBuffers failed: Invalid device.");
		return false;
	}

	HRESULT result;

	// Get swap chain description
	DXGI_SWAP_CHAIN_DESC swcDesc = {};	// Swap chain description structure
	result = m_pSwapChain->GetDesc(&swcDesc);

	for (UINT idx = 0; idx < swcDesc.BufferCount; idx++)
	{
		auto& buffer = m_backBuffers[idx];
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };	// Back buffer clear color (black)

		// Get render target view handle
		auto rtvIndex = m_descriptorHeapAllocator->AllocateRtv();				// Allocate render target view index
		auto rtvHandle = m_descriptorHeapAllocator->GetRtvCpuHandle(rtvIndex);	// Get render target view handle

		// Create render target view (RTV)
		// Render target view settings
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};	// Render target view settings structure

		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// Color format
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;	// 2D texture

		// Get buffer from swap chain
		result = m_pSwapChain->GetBuffer(
			idx,													// Index of the buffer to get
			IID_PPV_ARGS(buffer.resource.ReleaseAndGetAddressOf())	// Get the address of the render target (specify the object type with IID_PPV_ARGS macro)
		);

		// Create render target view (RTV)
		m_device->CreateRenderTargetView(
			buffer.resource.Get(),		// Buffer to set as render target
			&rtvDesc,					// Render target view settings (for sRGB)
			rtvHandle					// Handle to the descriptor heap to store the render target view
		);

		buffer.rtvIndex = rtvIndex;							// Save the RTV index of the back buffer
		buffer.currentState = D3D12_RESOURCE_STATE_COMMON;	// Save the current resource state of the back buffer
		buffer.clearColor[0] = clearColor[0];				// Save the clear color of the back buffer
		buffer.clearColor[1] = clearColor[1];
		buffer.clearColor[2] = clearColor[2];
		buffer.clearColor[3] = clearColor[3];
	}

	return true;
}