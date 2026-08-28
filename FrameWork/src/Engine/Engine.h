#pragma once
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>

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
#include "Engine/Graphics/SwapChain.h"
#include "Engine/Graphics/FrameCommandManager.h"
#include "Engine/Resource/GpuTexture.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

class TextureManager;

using RenderTargetHandle = uint32_t;
static constexpr RenderTargetHandle InvalidRenderTargetHandle = UINT32_MAX;

// Render pass target types (Built-in render target or back buffer)
enum class RenderPassTargetType
{
	BackBuffer,	// Back buffer render target (for presenting to the screen)
	ColorDepth,	// Both color and depth (RTV and DSV)
	DepthOnly,	// Depth only (DSV only)
	Count
};

// Render pass target structure (used to specify the render target for rendering)
struct RenderPassTarget
{
	static constexpr uint32_t InvalidIndex = UINT32_MAX;

	RenderPassTargetType type = RenderPassTargetType::BackBuffer;
	uint32_t colorIndex = InvalidIndex;	// For BackBuffer, this is the buffer index; for Builtin, this is the built-in render target index
	uint32_t depthIndex = InvalidIndex;	

	bool clearColor = true;	// Whether to clear the color buffer
	bool clearDepth = true;	// Whether to clear the depth buffer
};

// DirectX12 engine class
class Engine
{
public:
	enum class BuiltinRenderTarget
	{
		ShadowMap = 0,	// Shadow map render target
		SceneColor,		// Main render target for the scene
		SceneDepth,		// Depth render target for the scene
		SelectionMask,	// Selection mask render target (for editor selection)
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
	void EndFrame();							// End rendering

	// Flush the GPU command queue and wait for completion
	bool FlushGPU();

	// Resize the window output resources.
	// Must be called outside command recording.
	bool ResizeOutput(UINT width, UINT height);

	// Various getters
	ID3D12Device* GetDevice() { return m_pDevice.Get(); }												// Get device
	ID3D12GraphicsCommandList* GetCommandList() { return m_pCurrentCommandList; }						// Get command list
	UINT GetCurrentBufferIndex() const { return m_swapChain.GetCurrentBackBufferIndex(); }				// Get frame buffer index
	DescriptorHeapAllocator* GetDescriptorHeapAllocator() { return &m_descriptorHeapAllocator; }			// Get descriptor heap allocator
	GpuTexture* GetBuiltinRenderTarget(BuiltinRenderTarget target) { return m_builtinRenderTargets[static_cast<size_t>(target)].get(); }	// Get built-in render target by enum
	UINT GetFrameBufferWidth() const { return m_frameBufferWidth; }										// Get frame buffer width
	UINT GetFrameBufferHeight() const { return m_frameBufferHeight; }									// Get frame buffer height

	// Resize the scene render targets (color and depth) to the specified width and height
	bool ResizeSceneRenderTargets(UINT width, UINT height);

private:
	// Window related
	HWND hwnd = nullptr;		// Window handle

private:	// DirectX12 related
	ComPtr<ID3D12Device> m_pDevice;						// Device
	DescriptorHeapAllocator m_descriptorHeapAllocator;	// Descriptor heap allocator (for CBV/SRV/UAV, RTV, DSV)
	FrameCommandManager m_frameCommandManager;			// Frame command manager
	SwapChain m_swapChain;								// Swap chain wrapper

	ID3D12GraphicsCommandList* m_pCurrentCommandList = nullptr;	// Current rendering command list (for the current frame)

	D3D12_VIEWPORT m_viewport{};	// Viewport
	D3D12_RECT m_scissorRect{};		// Scissor rectangle


private:	// Rendering related
	UINT m_frameBufferWidth = 0;		// Frame buffer width
	UINT m_frameBufferHeight = 0;		// Frame buffer height

	// Resource management
	std::array< std::unique_ptr<GpuTexture>, static_cast<size_t>(BuiltinRenderTarget::Count)> m_builtinRenderTargets{};	// Built-in render target handles (post-processing, bloom, motion blur, etc.)
	RenderTargetHandle m_nextRenderTargetHandle = static_cast<RenderTargetHandle>(BuiltinRenderTarget::Count);			// Next render target handle to assign
	TextureManager* m_pTextureManager = nullptr;	// Texture manager (for post-processing render target)

private:	// Result code
	HRESULT result = S_OK;	// HRESULT (success/failure code)

private:
	// Various creation functions
	void CreateDevice();					// Device creation
	void CreateDescriptorHeapAllocator();	// Descriptor heap allocator creation
	void CreateViewport();					// Viewport creation
	void CreateScissorRect();				// Scissor rectangle creation
	void CreateBuiltinRenderTargets();		// Built-in render target creation (post-processing, bloom, motion blur, etc.)

	// Built-in render target creation functions
	void CreateSceneDepthRenderTarget();	// Scene depth render target creation
	void CreateShadowMapRenderTarget();		// Shadow map render target creation
	void CreatePostProcessRenderTarget();	// Post-processing render target creation
	void CreateSelectionMaskRenderTarget();	// Selection mask render target creation

	void SetViewPortAndScissorRect(const GpuTexture& renderTarget);	// Set viewport and scissor rectangle based on the render target
};
