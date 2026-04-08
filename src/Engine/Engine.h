#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <tchar.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <vector>
#include <string>
#include <array>
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine/Graphics/DescriptorHeapAllocator.h"
#include "Engine/Resource/RenderTargetTexture.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class TextureManager;

using RenderTargetHandle = uint32_t;
static constexpr RenderTargetHandle InvalidRenderTargetHandle = UINT32_MAX;

// Render pass target types (Built-in render target or back buffer)
enum class RenderPassTargetType
{
	BackBuffer,
	Builtin,
};

// Render pass target structure (used to specify the render target for rendering)
struct RenderPassTarget
{
	RenderPassTargetType type;
	uint32_t index;	// For BackBuffer, this is the buffer index; for Builtin, this is the built-in render target index
};

// Back buffer render target structure
struct BackBufferRenderTarget
{
	ComPtr<ID3D12Resource> resource;									// Back buffer resource
	uint32_t rtvIndex;													// RTV index for the back buffer
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;	// Current resource state of the back buffer
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };					// Clear color for the back buffer
};

// DirectX12 engine class
class Engine
{
public:
	static constexpr int FRAME_BUFFER_COUNT = 2; // Number of frame buffers for double buffering

	enum class BuiltinRenderTarget
	{
		SceneColor = 0,	// Main render target for the scene
		//BloomA,
		//BloomB,
		//MotionBlur,
		Count
	};

public:
	Engine() = default;
	~Engine() = default;

	// Main processing function
	bool InitCore(			// Initialization
		HWND hwnd,						// Window handle
		UINT m_FrameBufferWidth,		// Frame buffer width
		UINT m_FrameBufferHeight		// Frame buffer height
	);
	void InitBindings(TextureManager* pTextureManager);	// Initialize bindings (root signature, descriptor heaps, etc.)
	void Terminate();			// Termination

	// Rendering related functions
	void BeginPass(RenderPassTarget target);	// Set up render target
	void EndPass(RenderPassTarget target);		// End render pass
	void BeginFrame();							// Start rendering
	void WaitRender();							// Wait for the previous frame to finish
	void RenderEnd();							// End rendering

	// Various getters
	ID3D12Device* GetDevice() { return m_pDevice.Get(); }												// Get device
	ID3D12GraphicsCommandList* GetCommandList() { return m_pCommandList.Get(); }						// Get command list
	UINT GetCurrentBufferIndex() const { return m_currentBackBufferIndex; }								// Get frame buffer index
	DescriptorHeapAllocator* GetDescriptorHeapAllocator() { return m_pDescriptorHeapAllocator.get(); }	// Get descriptor heap allocator
	RenderTargetTexture* GetBuiltinRenderTarget(BuiltinRenderTarget target) { return m_builtinRenderTargets[static_cast<size_t>(target)].get(); }	// Get built-in render target by enum

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

	// Resource management
	BackBufferRenderTarget m_backBuffers[FRAME_BUFFER_COUNT];																		// Back buffers
	ComPtr<ID3D12Resource> m_pDepthStencilBuffer = nullptr;																			// Depth stencil buffer (only one is needed)
	std::array< std::unique_ptr<RenderTargetTexture>, static_cast<size_t>(BuiltinRenderTarget::Count)> m_builtinRenderTargets{};	// Built-in render target handles (post-processing, bloom, motion blur, etc.)
	std::unique_ptr<DescriptorHeapAllocator> m_pDescriptorHeapAllocator;															// Descriptor heap allocator (for CBV/SRV/UAV, RTV, DSV)	
	RenderTargetHandle m_nextRenderTargetHandle = static_cast<RenderTargetHandle>(BuiltinRenderTarget::Count);						// Next render target handle to assign
	TextureManager* m_pTextureManager = nullptr;	// Texture manager (for post-processing render target)

private:	// Result code
	HRESULT result = S_OK;	// HRESULT (success/failure code)

private:	// Internal functions
	// Various creation functions
	void CreateDevice();					// Device creation
	void CreateDescriptorHeapAllocator();	// Descriptor heap allocator creation
	void CreateCommandObjects();			// Command object creation
	void CreateSwapChain();					// Swap chain creation
	void CreateFence();						// Fence creation
	void CreateViewport();					// Viewport creation
	void CreateScissorRect();				// Scissor rectangle creation
	void CreateBackBuffers();				// Back buffer creation
	void CreateBuiltinRenderTargets();		// Built-in render target creation (post-processing, bloom, motion blur, etc.)
	void CreateDepthStencil();				// Depth stencil creation

	// Built-in render target creation functions
	void CreatePostProcessRenderTarget();	// Post-processing render target creation

};