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
#include <DirectXTex.h>
#include <vector>
#include <string>
#include "Engine/Core/ComPtr/ComPtr.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class TextureManager;

// Render target types
enum class RENDER_TARGET_TYPE
{
	BACK_BUFFER_0 = 0,	// Back buffer render target 0
	BACK_BUFFER_1,		// Back buffer render target 1
	POST_PROCESS,		// Post-processing render target
	TYPE_COUNT,			// Number of types
};

// Render target slot structure
struct RenderTargetSlot
{
	ComPtr<ID3D12Resource> renderTarget = { nullptr };			// Render targets(Back buffer + post-processing)
	uint32_t rtvIndex = 0;											// RTV descriptor index (for back buffer, it is the same as the back buffer index; for post-processing, it is a fixed index)
	D3D12_RESOURCE_STATES m_currenttargetState{};					// Render target states(Back buffer + post-processing)
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };				// Clear color (RGBA)
};

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
	bool InitCore(			// Initialization
		HWND hwnd,						// Window handle
		UINT m_FrameBufferWidth,		// Frame buffer width
		UINT m_FrameBufferHeight		// Frame buffer height
	);
	void InitBindings(TextureManager* pTextureManager);	// Initialize bindings (root signature, descriptor heaps, etc.)
	void Terminate();			// Termination

	// Rendering related functions
	void BeginPass(RENDER_TARGET_TYPE type);	// Set up render target
	void EndPass(RENDER_TARGET_TYPE type);		// End render pass
	void BeginFrame();							// Start rendering
	void WaitRender();							// Wait for the previous frame to finish
	void RenderEnd();							// End rendering

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

	// Render target related
	RenderTargetSlot m_renderTargetSlots[static_cast<int>(RENDER_TARGET_TYPE::TYPE_COUNT)] = {};	// Render target slots (back buffer + post-processing)
	ComPtr<ID3D12DescriptorHeap> m_pRTVHeap = nullptr;												// Current frame's RTV descriptor heap (temporarily stored)
	uint32_t m_rtvDescriptorSize = 0;																// RTV descriptor size (temporarily stored)

	// Depth stencil
	UINT m_dsvDescriptorSize = 0;							// Depth stencil descriptor size
	ComPtr<ID3D12DescriptorHeap> m_pDsvHeap = nullptr;		// Depth stencil descriptor heap
	ComPtr<ID3D12Resource> m_pDepthStencilBuffer = nullptr;	// Depth stencil buffer (only one is needed)

	TextureManager* m_pTextureManager = nullptr;	// Texture manager (for post-processing render target)
private:	// Result code
	HRESULT result = S_OK;	// HRESULT (success/failure code)

private:	// Internal functions
	// Various creation functions
	void CreateDevice();					// Device creation
	void CreateCommandObjects();			// Command object creation
	void CreateSwapChain();					// Swap chain creation
	void CreateFence();						// Fence creation
	void CreateViewport();					// Viewport creation
	void CreateScissorRect();				// Scissor rectangle creation
	void CreateRTVHeap();					// RTV descriptor heap creation
	void CreateRenderTarget();				// Render target creation
	void CreatePostProcessRenderTarget();	// Post-processing render target creation
	void CreateDepthStencil();				// Depth stencil creation

	RenderTargetSlot& GetRenderTargetSlot(RENDER_TARGET_TYPE type);
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t idx);
};