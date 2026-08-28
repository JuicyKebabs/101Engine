#include "SwapChain.h"
#include "Engine/Graphics/DescriptorHeapAllocator.h"
#include "Engine/Core/Debug/Debug.h"

#include <cassert>
#include <stdexcept>

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

	if (!CreateSwapChain(pCommandQueue, hwnd, width, height))
	{
		DBG("SwapChain::Initialize failed: CreateSwapChain failed.");
		return false;
	}

	if (!CreateBackBuffers())
	{
		DBG("SwapChain::Initialize failed: CreateBackBuffers failed.");
		return false;
	}

	return true;
}

HRESULT SwapChain::Present(UINT syncInterval, UINT flags)
{
	if (!m_pSwapChain) return E_POINTER;

	return m_pSwapChain->Present(syncInterval, flags);
}

bool SwapChain::Resize(UINT width, UINT height)
{
	if (!m_pSwapChain)
	{
		DBG("SwapChain::Resize failed: Swap chain is not initialized.");
		return false;
	}

	if (width == 0 || height == 0)
	{
		DBG("SwapChain::Resize failed: Invalid width or height.");
		return false;
	}

	// Get the current swap chain description to check if a resize is necessary
	DXGI_SWAP_CHAIN_DESC1 swcDesc = {};
	HRESULT hr = m_pSwapChain->GetDesc1(&swcDesc);

	if (FAILED(hr))
	{
		DBG("SwapChain::Resize failed: GetDesc1 failed with HRESULT 0x%08X.", hr);
		return false;
	}

	if (swcDesc.Width == width && swcDesc.Height == height)
	{
		DBG("SwapChain::Resize: No resize needed, dimensions are the same.");
		return true; // No need to resize if dimensions are the same
	}

	// Release the back buffer resources before resizing
	for (auto& buffer : m_backBuffers)
	{
		if (buffer.resource)
		{
			buffer.resource.Reset();
		}
	}

	// Resize the swap chain buffers
	HRESULT result = m_pSwapChain->ResizeBuffers(
		BufferCount,
		width,
		height,
		swcDesc.Format,
		swcDesc.Flags
	);

	if (FAILED(result))
	{
		DBG("SwapChain::Resize failed: ResizeBuffers failed with HRESULT 0x%08X.", result);
		return false;
	}

	// Recreate the back buffers after resizing
	if (!CreateBackBuffers())
	{
		DBG("SwapChain::Resize failed: CreateBackBuffers failed after resizing.");
		return false;
	}

	return true;
}

BackBufferRenderTarget& SwapChain::GetBackBuffer(UINT index)
{
	if (index >= BufferCount)
	{
		assert(false && "SwapChain::GetBackBuffer: Index out of bounds.");
		throw std::out_of_range("SwapChain::GetBackBuffer: Index out of bounds.");
	}

	return m_backBuffers[index];
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

	// The engine handles Alt+Enter itself to provide borderless fullscreen.
	// Disable DXGI's default exclusive-fullscreen transition to avoid both
	// mode-change paths running for the same key press.
	result = factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	if (FAILED(result))
	{
		DBG("SwapChain::Initialize failed: MakeWindowAssociation failed with HRESULT 0x%08X.", result);
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
	if (!m_device || !m_descriptorHeapAllocator || !m_pSwapChain)
	{
		DBG("SwapChain::CreateBackBuffers failed: Invalid dependencies.");
		return false;
	}

	HRESULT result;

	// Get swap chain description
	DXGI_SWAP_CHAIN_DESC swcDesc = {};	// Swap chain description structure
	result = m_pSwapChain->GetDesc(&swcDesc);

	if (FAILED(result))
	{
		DBG("SwapChain::CreateBackBuffers failed: GetDesc failed with HRESULT 0x%08X.", result);
		return false;
	}

	for (UINT idx = 0; idx < swcDesc.BufferCount; idx++)
	{
		auto& buffer = m_backBuffers[idx];
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };	// Back buffer clear color (black)

		// Get render target view handle
		if (buffer.rtvIndex == BackBufferRenderTarget::InvalidIndex)
		{
			buffer.rtvIndex = m_descriptorHeapAllocator->AllocateRtv();	// Allocate RTV index if not already allocated
		}

		auto rtvHandle = m_descriptorHeapAllocator->GetRtvCpuHandle(buffer.rtvIndex);	// Get render target view handle

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

		if (FAILED(result))
		{
			DBG("SwapChain::CreateBackBuffers failed: GetBuffer failed with HRESULT 0x%08X.", result);
			return false;
		}

		// Create render target view (RTV)
		m_device->CreateRenderTargetView(
			buffer.resource.Get(),		// Buffer to set as render target
			&rtvDesc,					// Render target view settings (for sRGB)
			rtvHandle					// Handle to the descriptor heap to store the render target view
		);

		buffer.currentState = D3D12_RESOURCE_STATE_COMMON;	// Save the current resource state of the back buffer
		buffer.clearColor[0] = clearColor[0];				// Save the clear color of the back buffer
		buffer.clearColor[1] = clearColor[1];
		buffer.clearColor[2] = clearColor[2];
		buffer.clearColor[3] = clearColor[3];
	}

	return true;
}
