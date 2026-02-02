#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <tchar.h>
//#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "DirectXTex.h"
#include <vector>
#include <string>
#include "ComPtr.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// DirectX12 engine class
class Engine
{
public:
	static constexpr int FRAME_BUFFER_COUNT = 2; // Number of frame buffers for double buffering

public:
	Engine();	// Constructor
	~Engine();	// Destructor

	// Get singleton instance
	static Engine* GetInstance()
	{
		static Engine instance;
		return &instance;
	}

	// Main processing function
	bool Initialize(			// Initialization
		HWND hwnd,					// Window handle
		UINT m_FrameBufferWidth,	// Frame buffer width
		UINT m_FrameBufferHeight	// Frame buffer height
	);
	void Terminate();			// Termination

	// Rendering related functions
	void RenderBegin();	// Start rendering
	void WaitRender();	// Wait for the previous frame to finish
	void RenderEnd();	// End rendering

	// Various getters
	ID3D12Device* GetDevice() { return m_pDevice.Get(); }							// Get device
	ID3D12GraphicsCommandList* GetCommandList() { return m_pCommandList.Get(); }	// Get command list
	UINT GetCurrentBufferIndex() const { return m_currentBackBufferIndex; }			// Get frame buffer index

private:
	// Window related
	HWND hwnd = nullptr;		// Window handle

private:	// DirectX12 related
	ComPtr<ID3D12Device> m_pDevice;												// Device
	ComPtr<ID3D12CommandAllocator> m_pCommandAllocator[FRAME_BUFFER_COUNT];		// Command allocator
	ComPtr<ID3D12GraphicsCommandList> m_pCommandList;							// Command list
	ComPtr<ID3D12CommandQueue> m_pCommandQueue;									// Command queue
	ComPtr<IDXGISwapChain4> m_pSwapChain;										// Swap chain

	HANDLE m_fenceEvent = nullptr;				// Fence event handle
	ComPtr<ID3D12Fence> m_pFence;				// Fence
	UINT64 m_fenceValue[FRAME_BUFFER_COUNT]{};	// Fence values

	D3D12_VIEWPORT m_viewport{};	// Viewport
	D3D12_RECT m_scissorRect{};		// Scissor rectangle

private:	// Rendering related
	UINT m_FrameBufferWidth = 0;		// Frame buffer width
	UINT m_FrameBufferHeight = 0;		// Frame buffer height
	UINT m_currentBackBufferIndex = 0;	// Current back buffer index

	ComPtr<ID3D12DescriptorHeap> m_pRTVHeap;									// RTV descriptor heap
	UINT m_rtvDescriptorSize = 0;												// RTV descriptor size
	ComPtr<ID3D12Resource> m_pRenderTargets[FRAME_BUFFER_COUNT] = { nullptr };	// Render targets (double buffering)

	UINT m_dsvDescriptorSize = 0;							// Depth stencil descriptor size
	ComPtr<ID3D12DescriptorHeap> m_pDsvHeap = nullptr;		// Depth stencil descriptor heap
	ComPtr<ID3D12Resource> m_pDepthStencilBuffer = nullptr;	// Depth stencil buffer (only one is needed)

	ID3D12Resource* m_currentRenderTarget = nullptr; // Current frame's render target (temporarily stored)

private:	// Result code
	HRESULT result = S_OK;	// HRESULT (success/failure code)

private:	// Internal functions
	// Various creation functions
	void CreateDevice();			// Device creation
	void CreateCommandObjects();	// Command object creation
	void CreateSwapChain();			// Swap chain creation
	void CreateFence();				// Fence creation
	void CreateViewport();			// Viewport creation
	void CreateScissorRect();		// Scissor rectangle creation
	void CreateRenderTarget();		// Render target creation
	void CreateDepthStencil();		// Depth stencil creation
};