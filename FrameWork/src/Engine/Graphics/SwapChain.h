#pragma once
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <array>
#include "Engine/Core/ComPtr/ComPtr.h"

class DescriptorHeapAllocator;

//--------------------
// SwapChain class
//--------------------

// Back buffer render target structure
struct BackBufferRenderTarget
{
	ComPtr<ID3D12Resource> resource;									// Back buffer resource
	uint32_t rtvIndex;													// RTV index for the back buffer
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;	// Current resource state of the back buffer
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };					// Clear color for the back buffer
};

class SwapChain
{
public:
	static constexpr UINT BufferCount = 2; // Number of frame buffers for double buffering

	bool Initialize(
		ID3D12Device* pDevice, 
		ID3D12CommandQueue* pCommandQueue, 
		DescriptorHeapAllocator* pDescriptorHeapAllocator,
		HWND hwnd, 
		UINT width, UINT height
	);

	HRESULT Present(UINT syncInterval, UINT flags);

	BackBufferRenderTarget& GetBackBuffer(UINT index);
	UINT GetCurrentBackBufferIndex() const;

private:
	ComPtr<IDXGISwapChain4> m_pSwapChain;	// Swap chain
	std::array<BackBufferRenderTarget, BufferCount> m_backBuffers;	// Back buffers

	ID3D12Device* m_device = nullptr;
	DescriptorHeapAllocator* m_descriptorHeapAllocator = nullptr;

private:
	bool CreateSwapChain(ID3D12CommandQueue* pCommandQueue, HWND hwnd, UINT width, UINT height);
	bool CreateBackBuffers();
};