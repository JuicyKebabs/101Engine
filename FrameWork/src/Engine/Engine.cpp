#include "Engine/Engine.h"
#include "Engine/Resource/TextureManager.h"

using namespace DirectX;

bool Engine::InitCore(HWND hwnd, UINT m_FrameBufferWidth, UINT m_FrameBufferHeight)
{
	this->hwnd = hwnd;									//ウィンドウハンドルの保存
	this->m_frameBufferWidth = m_FrameBufferWidth;		//フレームバッファの幅の保存
	this->m_frameBufferHeight = m_FrameBufferHeight;	//フレームバッファの高さの保存

	CreateDevice();						//デバイスの生成
	CreateDescriptorHeapAllocator();	//ディスクリプタヒープアロケータの生成
	CreateCommandObjects();				//コマンドオブジェクトの生成

	bool result = m_swapChain.Initialize(
		m_pDevice.Get(),
		m_pCommandQueue.Get(),
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


	CreateFence();						//フェンスの生成
	CreateViewport();					//ビューポートの生成
	CreateScissorRect();				//シザー矩形の生成
	CreateBuiltinRenderTargets();		//ビルトインレンダーターゲットの生成
	return true;
}

void Engine::InitBindings(TextureManager* pTextureManager)
{
	this->m_pTextureManager = pTextureManager; 
}

//終了
void Engine::Terminate()
{
	//フェンスイベントのクローズ
	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
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
		m_pCommandList->ResourceBarrier(1, &barrier);

		D3D12_VIEWPORT viewport = {};
		viewport.Width = static_cast<float>(m_frameBufferWidth);
		viewport.Height = static_cast<float>(m_frameBufferHeight);
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MaxDepth = 1.0f;
		viewport.MinDepth = 0.0f;
		m_pCommandList->RSSetViewports(1, &viewport);

		D3D12_RECT scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = m_frameBufferWidth;
		scissorRect.bottom = m_frameBufferHeight;
		m_pCommandList->RSSetScissorRects(1, &scissorRect);

		const auto rtvHandle = m_descriptorHeapAllocator.GetRtvCpuHandle(rtvIndex);	// Get the RTV handle for the current render target slot

		m_pCommandList->OMSetRenderTargets(
			1,
			&rtvHandle,
			FALSE,
			nullptr
		);

		// Clear the render target view when required
		if (target.clearColor)
		{
			m_pCommandList->ClearRenderTargetView(
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

		color.TransitionToState(m_pCommandList.Get(), GpuTexture::ResourceState::RenderTarget);
		depth.TransitionToState(m_pCommandList.Get(), GpuTexture::ResourceState::DepthWrite);

		SetViewPortAndScissorRect(color);	// Set the viewport and scissor rectangle for rendering

		const auto rtvHandle = m_descriptorHeapAllocator.GetRtvCpuHandle(color.GetRtvIndex());	// Get the RTV handle for the current render target slot
		const auto dsvHandle = m_descriptorHeapAllocator.GetDsvCpuHandle(depth.GetDsvIndex());	// Get the DSV handle for the depth render target

		m_pCommandList->OMSetRenderTargets(
			1,
			&rtvHandle,
			FALSE,
			&dsvHandle
		);

		// Clear the render target view and depth stencil view when required
		if (target.clearColor)
		{
			m_pCommandList->ClearRenderTargetView(
				rtvHandle,
				color.GetClearColor(),
				0,
				nullptr
			);
		}

		if (target.clearDepth)
		{
			m_pCommandList->ClearDepthStencilView(
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

		depth.TransitionToState(m_pCommandList.Get(), GpuTexture::ResourceState::DepthWrite);

		SetViewPortAndScissorRect(depth);	// Set the viewport and scissor rectangle for rendering

		const auto dsvHandle = m_descriptorHeapAllocator.GetDsvCpuHandle(depth.GetDsvIndex());	// Get the DSV handle for the depth-only render target

		m_pCommandList->OMSetRenderTargets(
			0,
			nullptr,
			FALSE,
			&dsvHandle
		);

		if (target.clearDepth)
		{
			m_pCommandList->ClearDepthStencilView(
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
		m_pCommandList->ResourceBarrier(1, &barrier);

		return;
	}
	else if (target.type == RenderPassTargetType::ColorDepth)
	{
		auto& color = *m_builtinRenderTargets.at(target.colorIndex);
		color.TransitionToState(m_pCommandList.Get(), GpuTexture::ResourceState::ShaderResource);

		return;
	}
	else if (target.type == RenderPassTargetType::DepthOnly)
	{
		auto& depth = *m_builtinRenderTargets.at(target.depthIndex);
		depth.TransitionToState(m_pCommandList.Get(), GpuTexture::ResourceState::ShaderResource);
		return;
	}
}

// Begin rendering the frame
void Engine::BeginFrame()
{
	// Initialize command settings for the current frame
	m_pCommandAllocator[m_swapChain.GetCurrentBackBufferIndex()]->Reset();		// Reset the command allocator for the current back buffer index
	m_pCommandList->Reset(										// Reset the command list
		m_pCommandAllocator[m_swapChain.GetCurrentBackBufferIndex()].Get(),	// Get the command allocator for the current back buffer index
		nullptr													// Initial pipeline state (nullptr means no initial pipeline state)
	);
}

// Wait for the GPU to finish rendering the current frame
void Engine::WaitRender()
{
	// Increment the fence value for the next frame
	const UINT64 fenceValue = m_nextFenceValue++;
	
	// Signal the command queue to set the fence value
	const HRESULT signalResult = m_pCommandQueue->Signal(m_pFence.Get(), fenceValue);

	// Check if signaling the fence was successful
	if (FAILED(signalResult))
	{
		assert(false && "Failed to signal GPU fence");
		return;
	}

	// If the fence has already been completed, no need to wait (return early)
	if (m_pFence->GetCompletedValue() >= fenceValue) return;

	// Set an event to be signaled when the fence reaches the specified value
	const HRESULT eventResult = m_pFence->SetEventOnCompletion(fenceValue, m_fenceEvent);

	// Check if setting the fence completion event was successful
	if (FAILED(eventResult))
	{
		assert(false && "Failed to set fence completion event");
		return;
	}

	// Wait for the fence event to be signaled (indicating that the GPU has finished rendering)
	const DWORD waitResult = WaitForSingleObject(m_fenceEvent, INFINITE);

	// Check if waiting for the fence event was successful
	if (waitResult != WAIT_OBJECT_0)
	{
		assert(false && "Failed while waiting for GPU fence");
	}
}

// End rendering the frame
void Engine::RenderEnd()
{
	// Close the command list
	m_pCommandList->Close();

	// Execute the command list
	ID3D12CommandList* cmdLists[] = { m_pCommandList.Get() };	// Array of command lists to execute
	m_pCommandQueue->ExecuteCommandLists(
		1,			// Number of command lists
		cmdLists	// Pointer to the array of command lists
	);

	// Swap the back buffers
	HRESULT result = m_swapChain.Present(1, 0);	// Present with vertical sync

	// Check if presenting the swap chain was successful
	if (FAILED(result))
	{
		assert(false && "Failed to present swap chain");
	}

	// Wait for the previous frame to finish
	WaitRender();
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
	WaitRender();

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

//コマンドオブジェクトの生成
void Engine::CreateCommandObjects()
{
	for (size_t i = 0; i < SwapChain::BufferCount; i++)
	{
		//コマンドアロケーターの生成
		result = m_pDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,			//直接コマンド
			IID_PPV_ARGS(&m_pCommandAllocator[i])	//コマンドアロケーターのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
		);

		if (FAILED(result))
		{
			assert(false && "Failed to create command allocator");
			return;
		}
	}

	result = m_pDevice->CreateCommandList(
		0,														//ノードマスク
		D3D12_COMMAND_LIST_TYPE_DIRECT,							//直接コマンド
		m_pCommandAllocator[m_swapChain.GetCurrentBackBufferIndex()].Get(),	//コマンドアロケーター
		nullptr,												//パイプラインステートオブジェクト
		IID_PPV_ARGS(&m_pCommandList)							//コマンドリストのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	if (FAILED(result))
	{
		assert(false && "Failed to create command list");
		return;
	}

	m_pCommandList->Close();	//コマンドリストは生成直後に開いている状態なので閉じておく

	//コマンドキューの生成
	//各種設定
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {}; //コマンドキューの設定構造体
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;				//フラグ
	cmdQueueDesc.NodeMask = 0;										//ノードマスク
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;	//優先度
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;				//直接コマンド(コマンドリストと同じ)

	//生成
	result = m_pDevice->CreateCommandQueue(
		&cmdQueueDesc,					//コマンドキューの設定構造体
		IID_PPV_ARGS(&m_pCommandQueue)	//コマンドキューのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	if (FAILED(result))
	{
		assert(false && "Failed to create command queue");
		return;
	}
}

void Engine::CreateFence()
{
	// Create a fence for GPU synchronization
	result = m_pDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pFence)
	);

	if (FAILED(result))
	{
		assert(false && "Failed to create fence");
		return;
	}

	m_nextFenceValue = 1;	// Initialize the next fence value
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
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
	m_pCommandList->RSSetViewports(1, &viewport);

	D3D12_RECT scissorRect = {};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = renderTarget.GetWidth();
	scissorRect.bottom = renderTarget.GetHeight();
	m_pCommandList->RSSetScissorRects(1, &scissorRect);
}