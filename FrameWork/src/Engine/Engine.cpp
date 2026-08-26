#include "Engine/Engine.h"
#include "Engine/Resource/TextureManager.h"

using namespace DirectX;

bool Engine::InitCore(HWND hwnd, UINT m_FrameBufferWidth, UINT m_FrameBufferHeight)
{
	this->hwnd = hwnd;
	this->m_frameBufferWidth = m_FrameBufferWidth;
	this->m_frameBufferHeight = m_FrameBufferHeight;

	CreateDevice();
	CreateDescriptorHeapAllocator();

	bool result = false;

	// Initialize the frame command manager
	result = m_frameCommandManager.Initialize(m_pDevice.Get(), SwapChain::BufferCount);

	if (!result)
	{
		assert(false && "Engine : Failed to initialize frame command manager");
		return false;
	}

	// Initialize the swap chain
	result = m_swapChain.Initialize(
		m_pDevice.Get(),
		m_frameCommandManager.GetNativeQueue(),
		&m_descriptorHeapAllocator,
		hwnd,
		m_frameBufferWidth,
		m_frameBufferHeight
	);

	if (!result)
	{
		assert(false && "Engine : Failed to initialize swap chain");
		return false;
	}

	CreateViewport();
	CreateScissorRect();
	CreateBuiltinRenderTargets();

	return true;
}

void Engine::InitBindings(TextureManager* pTextureManager)
{
	this->m_pTextureManager = pTextureManager; 
}

void Engine::Terminate()
{
	const bool flushed = FlushGPU();
	assert(flushed && "Failed to flush GPU commands");
}

// Begin rendering to the render target
void Engine::BeginPass(RenderPassTarget target)
{
	const auto builtinCount = static_cast<uint32_t>(BuiltinRenderTarget::Count);

	switch (target.type)
	{
	case RenderPassTargetType::BackBuffer:
		assert(target.colorIndex < SwapChain::BufferCount);
		assert(target.depthIndex == RenderPassTarget::InvalidIndex);
		break;

	case RenderPassTargetType::ColorDepth:
		assert(target.colorIndex < builtinCount);
		assert(target.depthIndex < builtinCount);
		break;

	case RenderPassTargetType::DepthOnly:
		assert(target.colorIndex == RenderPassTarget::InvalidIndex);
		assert(target.depthIndex < builtinCount);
		break;

	default:
		assert(false && "Invalid render pass target type");
		return;
	}

	ID3D12Resource* resource = nullptr;
	uint32_t rtvIndex = 0;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES nextState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Set up the render target based on the target type
	if(target.type == RenderPassTargetType::BackBuffer)
	{
		auto& rt = m_swapChain.GetBackBuffer(target.colorIndex);				// Get the back buffer render target for the specified index
		resource = rt.resource.Get();						// Get the resource for the back buffer
		rtvIndex = rt.rtvIndex;
		currentState = rt.currentState;						// Get the current resource state of the back buffer
		clearColor[0] = rt.clearColor[0];
		clearColor[1] = rt.clearColor[1];
		clearColor[2] = rt.clearColor[2];
		clearColor[3] = rt.clearColor[3];

		rt.currentState = GpuTexture::ConvertToD3D12State(GpuTexture::ResourceState::RenderTarget);	// Update the current state of the back buffer to "Write" for rendering
	
		// Set up the resource barrier to render target state
		auto barrier =
			CD3DX12_RESOURCE_BARRIER::Transition(
				resource,		// Current render target resource
				currentState,	// Current resource state
				nextState		// New resource state for rendering
			);
		// Set the resource barrier command
		m_pCurrentCommandList->ResourceBarrier(1, &barrier);

		D3D12_VIEWPORT viewport = {};
		viewport.Width = static_cast<float>(m_frameBufferWidth);
		viewport.Height = static_cast<float>(m_frameBufferHeight);
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MaxDepth = 1.0f;
		viewport.MinDepth = 0.0f;
		m_pCurrentCommandList->RSSetViewports(1, &viewport);

		D3D12_RECT scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = m_frameBufferWidth;
		scissorRect.bottom = m_frameBufferHeight;
		m_pCurrentCommandList->RSSetScissorRects(1, &scissorRect);

		const auto rtvHandle = m_descriptorHeapAllocator.GetRtvCpuHandle(rtvIndex);	// Get the RTV handle for the current render target slot

		m_pCurrentCommandList->OMSetRenderTargets(
			1,
			&rtvHandle,
			FALSE,
			nullptr
		);

		// Clear the render target view when required
		if (target.clearColor)
		{
			m_pCurrentCommandList->ClearRenderTargetView(
				rtvHandle,
				clearColor,
				0,
				nullptr
			);
		}

		return;
	}
	else if (target.type == RenderPassTargetType::ColorDepth)
	{// Color depth uses render target, and next state is RenderTrget
		auto& color = *m_builtinRenderTargets.at(target.colorIndex);

		auto& depth = *m_builtinRenderTargets.at(target.depthIndex);

		assert(color.GetWidth() == depth.GetWidth());
		assert(color.GetHeight() == depth.GetHeight());

		color.TransitionToState(m_pCurrentCommandList, GpuTexture::ResourceState::RenderTarget);
		depth.TransitionToState(m_pCurrentCommandList, GpuTexture::ResourceState::DepthWrite);

		SetViewPortAndScissorRect(color);	// Set the viewport and scissor rectangle for rendering

		const auto rtvHandle = m_descriptorHeapAllocator.GetRtvCpuHandle(color.GetRtvIndex());	// Get the RTV handle for the current render target slot
		const auto dsvHandle = m_descriptorHeapAllocator.GetDsvCpuHandle(depth.GetDsvIndex());	// Get the DSV handle for the depth render target

		m_pCurrentCommandList->OMSetRenderTargets(
			1,
			&rtvHandle,
			FALSE,
			&dsvHandle
		);

		// Clear the render target view and depth stencil view when required
		if (target.clearColor)
		{
			m_pCurrentCommandList->ClearRenderTargetView(
				rtvHandle,
				color.GetClearColor(),
				0,
				nullptr
			);
		}

		if (target.clearDepth)
		{
			m_pCurrentCommandList->ClearDepthStencilView(
				dsvHandle,
				D3D12_CLEAR_FLAG_DEPTH,
				depth.GetClearDepth(),
				0,
				0,
				nullptr
			);
		}

		return;
	}
	else if (target.type == RenderPassTargetType::DepthOnly)
	{// Depth only uses depth buffer, and next state is DepthWrite
		auto& depth = *m_builtinRenderTargets.at(target.depthIndex);

		depth.TransitionToState(m_pCurrentCommandList, GpuTexture::ResourceState::DepthWrite);

		SetViewPortAndScissorRect(depth);	// Set the viewport and scissor rectangle for rendering

		const auto dsvHandle = m_descriptorHeapAllocator.GetDsvCpuHandle(depth.GetDsvIndex());	// Get the DSV handle for the depth-only render target

		m_pCurrentCommandList->OMSetRenderTargets(
			0,
			nullptr,
			FALSE,
			&dsvHandle
		);

		if (target.clearDepth)
		{
			m_pCurrentCommandList->ClearDepthStencilView(
				dsvHandle,
				D3D12_CLEAR_FLAG_DEPTH,
				depth.GetClearDepth(),
				0,
				0,
				nullptr
			);
		}

		return;
	}
}

// End rendering to the render target
void Engine::EndPass(RenderPassTarget target)
{
	const auto builtinCount = static_cast<uint32_t>(BuiltinRenderTarget::Count);

	switch (target.type)
	{
	case RenderPassTargetType::BackBuffer:
		assert(target.colorIndex < SwapChain::BufferCount);
		assert(target.depthIndex == RenderPassTarget::InvalidIndex);
		break;

	case RenderPassTargetType::ColorDepth:
		assert(target.colorIndex < builtinCount);
		assert(target.depthIndex < builtinCount);
		break;

	case RenderPassTargetType::DepthOnly:
		assert(target.colorIndex == RenderPassTarget::InvalidIndex);
		assert(target.depthIndex < builtinCount);
		break;

	default:
		assert(false && "Invalid render pass target type");
		return;
	}

	ID3D12Resource* resource = nullptr;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES nextState = D3D12_RESOURCE_STATE_COMMON;

	if(target.type == RenderPassTargetType::BackBuffer)
	{
		auto& rt = m_swapChain.GetBackBuffer(target.colorIndex);		// Get the back buffer render target for the specified index
		resource = rt.resource.Get();						// Get the resource for the back buffer
		currentState = rt.currentState;						// Get the current resource state of the back buffer
		nextState = D3D12_RESOURCE_STATE_PRESENT;			// Next state for the back buffer is "Present" for presentation to the screen
		rt.currentState = D3D12_RESOURCE_STATE_PRESENT;		// Update the current state of the back buffer to "Present" for post-processing
		
		// Set up the resource barrier to transition to the next state
		auto barrier =
			CD3DX12_RESOURCE_BARRIER::Transition(
				resource,		// Current render target
				currentState,	// Current resource state
				nextState		// New resource state for post-processing
			);

		// Set the resource barrier command
		m_pCurrentCommandList->ResourceBarrier(1, &barrier);

		return;
	}
	else if (target.type == RenderPassTargetType::ColorDepth)
	{
		auto& color = *m_builtinRenderTargets.at(target.colorIndex);
		color.TransitionToState(m_pCurrentCommandList, GpuTexture::ResourceState::ShaderResource);

		return;
	}
	else if (target.type == RenderPassTargetType::DepthOnly)
	{
		auto& depth = *m_builtinRenderTargets.at(target.depthIndex);
		depth.TransitionToState(m_pCurrentCommandList, GpuTexture::ResourceState::ShaderResource);
		return;
	}
}

// Begin rendering the frame
void Engine::BeginFrame()
{
	// Get the current back buffer index from the swap chain
	const size_t frameIndex = m_swapChain.GetCurrentBackBufferIndex();

	// Begin the frame command list for the current frame index
	// (Synchronization with the GPU and Resetting the command context)
	m_pCurrentCommandList = m_frameCommandManager.BeginFrame(frameIndex);

	if (!m_pCurrentCommandList)
	{
		assert(false && "Failed to begin frame command list");
		return;
	}
}

// End rendering the frame
void Engine::EndFrame()
{
	size_t frameIndex = m_swapChain.GetCurrentBackBufferIndex();

	m_pCurrentCommandList = nullptr;	// Clear the current command list pointer

	// End the frame command list for the current frame index
	// (Finalizing the command list and submitting it to the GPU with signaling)
	if (!m_frameCommandManager.EndFrame(frameIndex))
	{
		assert(false && "Failed to end frame command list");
		return;
	}

	// Present the frame (sync interval = 1 for VSync)
	const HRESULT hr = m_swapChain.Present(1, 0);
	if (FAILED(hr))
	{
		assert(false && "Failed to present swap chain");
	}
}

// Wait for the GPU to finish rendering the current frame
bool Engine::FlushGPU()
{
	if (!m_frameCommandManager.Flush())
	{
		assert(false && "Failed to flush GPU commands");
		return false;
	}

	return true;
}

bool Engine::ResizeSceneRenderTargets(UINT width, UINT height)
{
	if (width == 0 || height == 0) return false;	// Invalid size, return false

	// Get the render targets (color, depth, and selection mask)
	auto* sceneColor = GetBuiltinRenderTarget(BuiltinRenderTarget::SceneColor);
	auto* sceneDepth = GetBuiltinRenderTarget(BuiltinRenderTarget::SceneDepth);
	auto* selectionMask = GetBuiltinRenderTarget(BuiltinRenderTarget::SelectionMask);

	if (!sceneColor || !sceneDepth || !selectionMask)
	{
		assert(false && "Scene render targets are not initialized");
		return false;
	}

	// Skip resizing to the same size
	if (sceneColor->GetWidth() == width &&
		sceneColor->GetHeight() == height &&
		sceneDepth->GetWidth() == width &&
		sceneDepth->GetHeight() == height &&
		selectionMask->GetWidth() == width &&
		selectionMask->GetHeight() == height)
	{
		return true;
	}

	// Wait for the GPU to finish rendering before resizing
	const bool flushed = FlushGPU();

	if (!flushed)
	{
		assert(false && "Failed to flush GPU commands before resizing render targets");
		return false;
	}

	// Resize the render targets
	const bool colorResult = sceneColor->Resize(m_pDevice.Get(), &m_descriptorHeapAllocator, width, height);
	if (!colorResult)
	{
		assert(false && "SceneColor resize failed");
		return false;
	}

	const bool depthResult = sceneDepth->Resize(m_pDevice.Get(), &m_descriptorHeapAllocator, width, height);
	if (!depthResult)
	{
		assert(false && "SceneDepth resize failed after SceneColor resize");
		return false;
	}

	const bool selectionMaskResult = selectionMask->Resize(m_pDevice.Get(), &m_descriptorHeapAllocator, width, height);
	if (!selectionMaskResult)
	{
		assert(false && "SelectionMask resize failed after SceneColor and SceneDepth resize");
		return false;
	}

	// Ensure that the resized render targets have the same dimensions
	const bool hasSameSize =
		sceneColor->GetWidth() == sceneDepth->GetWidth() &&
		sceneColor->GetHeight() == sceneDepth->GetHeight() &&
		sceneColor->GetWidth() == selectionMask->GetWidth() &&
		sceneColor->GetHeight() == selectionMask->GetHeight();

	assert(hasSameSize);
	return hasSameSize;
}

//デバイスの生成
void Engine::CreateDevice()
{
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_1; //FeatureLevel

	//FeatureLevelを下げていき、対応しているものを探す
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	//対応しているFeatureLevelを探す
	for (auto lv : levels)
	{
		//デバイスを生成
		result = D3D12CreateDevice(
			nullptr,					//Adapterをnullptrにすると、既定のアダプターが使われる
			lv,							//FeatureLevelを指定
			IID_PPV_ARGS(&m_pDevice));	//デバイスのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)

		//成功判定
		if (result == S_OK)
		{//対応している場合はループを抜ける
			break;
		}
	}
}

// Create the descriptor heap allocator
void Engine::CreateDescriptorHeapAllocator()
{
	m_descriptorHeapAllocator.Initialize(m_pDevice.Get());
}

void Engine::CreateViewport()
{
	// View port settings
	D3D12_VIEWPORT viewport = {};

	viewport.Width = static_cast<float>(m_frameBufferWidth);	// Output width
	viewport.Height = static_cast<float>(m_frameBufferHeight);	// Output height

	viewport.TopLeftX = 0;		// Output top-left X coordinate
	viewport.TopLeftY = 0;		// Output top-left Y coordinate
	viewport.MaxDepth = 1.0f;	// Maximum depth
	viewport.MinDepth = 0.0f;	// Minimum depth

	m_viewport = viewport;	// Save to member variable
}

void Engine::CreateScissorRect()
{
	// Scissor rectangle settings
	D3D12_RECT scissorRect = {};	// Scissor rectangle settings structure

	scissorRect.top = 0;						// Top coordinate
	scissorRect.left = 0;						// Left coordinate
	scissorRect.right = m_frameBufferWidth;		// Right coordinate
	scissorRect.bottom = m_frameBufferHeight;	// Bottom coordinate

	m_scissorRect = scissorRect;	// Save to member variable
}

void Engine::CreateBuiltinRenderTargets()
{
	for(auto& target : m_builtinRenderTargets) 
	{
		target = std::make_unique<GpuTexture>();
	}

	CreatePostProcessRenderTarget();
	CreateSceneDepthRenderTarget();
	CreateShadowMapRenderTarget();
	CreateSelectionMaskRenderTarget();
}

void Engine::CreateSceneDepthRenderTarget()
{
	GpuTexture::ParamDesc desc{};

	desc.width = m_frameBufferWidth;
	desc.height = m_frameBufferHeight;

	desc.depthFormat = GpuTexture::DepthFormat::D32F;
	desc.initialState = GpuTexture::ResourceState::DepthWrite;

	desc.useRTV = false;
	desc.useDSV = true;
	desc.useSRV = false;
	desc.useUAV = false;

	desc.clearDepth = 1.0f;

	auto& sceneDepth = m_builtinRenderTargets[static_cast<size_t>(BuiltinRenderTarget::SceneDepth)];

	sceneDepth->Initialize(m_pDevice.Get(), &m_descriptorHeapAllocator, desc);
}

void Engine::CreateShadowMapRenderTarget()
{
	GpuTexture::ParamDesc desc{};
	desc.width = 2048;
	desc.height = 2048;
	desc.initialState = GpuTexture::ResourceState::ShaderResource;
	desc.depthFormat = GpuTexture::DepthFormat::D32F;
	desc.useDSV = true;
	desc.useSRV = true;

	auto& rt = m_builtinRenderTargets[static_cast<size_t>(BuiltinRenderTarget::ShadowMap)];
	rt->Initialize(m_pDevice.Get(), &m_descriptorHeapAllocator, desc);
}

// Create post-process render target
void Engine::CreatePostProcessRenderTarget()
{
	GpuTexture::ParamDesc desc{};
	desc.width = m_frameBufferWidth;
	desc.height = m_frameBufferHeight;
	desc.initialState = GpuTexture::ResourceState::ShaderResource;
	desc.format = GpuTexture::ColorFormat::RGBA16F;
	desc.clearColor[0] = 0.0f;
	desc.clearColor[1] = 0.0f;
	desc.clearColor[2] = 1.0f;
	desc.clearColor[3] = 1.0f;
	desc.useRTV = true;
	desc.useSRV = true;

	auto& rt = m_builtinRenderTargets[static_cast<size_t>(BuiltinRenderTarget::SceneColor)];

	rt->Initialize(m_pDevice.Get(), &m_descriptorHeapAllocator, desc);
}

void Engine::CreateSelectionMaskRenderTarget()
{
	GpuTexture::ParamDesc desc{};

	desc.width = m_frameBufferWidth;
	desc.height = m_frameBufferHeight;

	desc.initialState = GpuTexture::ResourceState::ShaderResource;
	desc.format = GpuTexture::ColorFormat::RGBA8;

	desc.clearColor[0] = 0.0f;
	desc.clearColor[1] = 0.0f;
	desc.clearColor[2] = 0.0f;
	desc.clearColor[3] = 0.0f;

	// Enable only RTV and SRV
	desc.useRTV = true;
	desc.useDSV = false;
	desc.useSRV = true;
	desc.useUAV = false;

	auto& selectionMask = m_builtinRenderTargets[static_cast<size_t>(BuiltinRenderTarget::SelectionMask)];

	selectionMask->Initialize(m_pDevice.Get(), &m_descriptorHeapAllocator, desc);
}

void Engine::SetViewPortAndScissorRect(const GpuTexture& renderTarget)
{
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(renderTarget.GetWidth());
	viewport.Height = static_cast<float>(renderTarget.GetHeight());
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;
	m_pCurrentCommandList->RSSetViewports(1, &viewport);

	D3D12_RECT scissorRect = {};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = renderTarget.GetWidth();
	scissorRect.bottom = renderTarget.GetHeight();
	m_pCurrentCommandList->RSSetScissorRects(1, &scissorRect);
}