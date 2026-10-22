#include "SwapChain.h"
#include "ApiResult.h"

#include <utility>

namespace rendering::backend
{

using Microsoft::WRL::ComPtr;

SwapChain::~SwapChain()
{
	Finalize();
}

Result SwapChain::Initialize(
	ID3D12Device* device,
	IDXGIFactory4* factory,
	ID3D12CommandQueue* commandQueue,
	const Win32WindowOutputDesc& outputDesc)
{
	if (IsInitialized())
	{
		return { false, "SwapChain is already initialized." };
	}

	if (device == nullptr || factory == nullptr || commandQueue == nullptr)
	{
		return { false, "SwapChain requires valid device, factory, and command queue." };
	}

	if (outputDesc.window == nullptr)
	{
		return { false, "SwapChain requires a valid window handle." };
	}

	if (outputDesc.width == 0 || outputDesc.height == 0)
	{
		return { false, "SwapChain requires non-zero width and height." };
	}

	if (commandQueue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		return { false, "SwapChain requires a direct command queue." };
	}

	// Create the swap chain in local scope to ensure proper cleanup in case of failure.
	DXGI_SWAP_CHAIN_DESC1 desc{};
	desc.Width = outputDesc.width;
	desc.Height = outputDesc.height;
	desc.Format = BackBufferFormat;
	desc.Stereo = FALSE;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = BufferCount;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	desc.Flags = 0;

	ComPtr<IDXGISwapChain1> swapChain1;

	HRESULT hr = factory->CreateSwapChainForHwnd(
		commandQueue,
		outputDesc.window,
		&desc,
		nullptr,
		nullptr,
		swapChain1.GetAddressOf());

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateSwapChainForHwnd", hr);
	}

	// Query for IDXGISwapChain4 interface
	ComPtr<IDXGISwapChain4> swapChain;

	hr = swapChain1.As(&swapChain);

	if (FAILED(hr))
	{
		return MakeApiFailure("QueryInterface<IDXGISwapChain4>", hr);
	}

	
	// Get the reference to the back buffers in the swap chain
	std::array<ComPtr<ID3D12Resource>, BufferCount> backBuffers{};

	for (UINT index = 0; index < BufferCount; ++index)
	{
		hr = swapChain->GetBuffer(index, IID_PPV_ARGS(backBuffers[index].GetAddressOf()));

		if (FAILED(hr))
		{
			return MakeApiFailure("SwapChain::GetBuffer", hr);
		}
	}

	// Create the RTV descriptor heap for back buffers
	const Result heapResult = m_rtvHeap.Initialize(
		device,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		BufferCount,
		false);

	if (!heapResult.IsSuccess())
	{
		return heapResult;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = BackBufferFormat;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	for (UINT index = 0; index < BufferCount; ++index)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};

		const Result handleResult = m_rtvHeap.GetCpuHandle(index, rtvHandle);

		if (!handleResult.IsSuccess())
		{
			m_rtvHeap.Finalize();
			return handleResult;
		}

		// Connect the RTV descriptor to the back buffer resource
		device->CreateRenderTargetView(backBuffers[index].Get(), &rtvDesc, rtvHandle);
	}

	// Disable the ALT+ENTER fullscreen toggle for the window associated with the swap chain
	hr = factory->MakeWindowAssociation(outputDesc.window, DXGI_MWA_NO_ALT_ENTER);

	if (FAILED(hr))
	{
		m_rtvHeap.Finalize();
		return MakeApiFailure("MakeWindowAssociation", hr);
	}

	// Move the created objects into the member variables
	m_swapChain = std::move(swapChain);
	for (UINT index = 0; index < BufferCount; ++index)
	{
		m_backBuffers[index].resource = std::move(backBuffers[index]);
		m_backBuffers[index].submittedState = D3D12_RESOURCE_STATE_PRESENT;
	}

	OutputDebugStringA("[101Renderer] SwapChain initialized.\n");

	return {};
}

void SwapChain::Finalize() noexcept
{
	for (auto& backBuffer : m_backBuffers)
	{
		backBuffer.resource.Reset();
		backBuffer.submittedState = D3D12_RESOURCE_STATE_PRESENT;
	}

	m_rtvHeap.Finalize();
	m_swapChain.Reset();
}

Result SwapChain::GetCurrentTexture(BackBufferInfo& output) const
{
	output = {};

	if (!IsInitialized())
	{
		return { false, "SwapChain: not initialized." };
	}

	const UINT index = m_swapChain->GetCurrentBackBufferIndex();

	if (index >= BufferCount)
	{
		return { false, "SwapChain: current buffer index is invalid." };
	}

	const auto& backBuffer = m_backBuffers[index];

	if (!backBuffer.resource)
	{
		return { false, "SwapChain: current resource is unavailable." };
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
	Result result = m_rtvHeap.GetCpuHandle(index, rtv);

	if (!result.IsSuccess())
	{
		return result;
	}

	BackBufferInfo info{};
	info.resource = backBuffer.resource.Get();
	info.rtv = rtv;
	info.viewFormat = BackBufferFormat;
	info.submittedState = backBuffer.submittedState;

	output = info;	
	return {};
}

void SwapChain::CommitSubmittedState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) noexcept
{
	for (auto& backBuffer : m_backBuffers)
	{
		if (resource && backBuffer.resource.Get() == resource)
		{
			backBuffer.submittedState = state;
			return;
		}
	}

	std::terminate(); // Owner identity was validated before submission.
}

Result SwapChain::Present(UINT syncInterval, UINT flags)
{
	if (!IsInitialized())
	{
		return { false, "SwapChain is not initialized." };
	}

	const HRESULT hr = m_swapChain->Present(syncInterval, flags);

	if (FAILED(hr))
	{
		return MakeApiFailure("SwapChain::Present", hr);
	}

	return {};
}

}