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
	CreateSwapChain();					//スワップチェーンの生成
	CreateFence();						//フェンスの生成
	CreateViewport();					//ビューポートの生成
	CreateScissorRect();				//シザー矩形の生成
	CreateBackBuffers();				//バックバッファの生成
	CreateBuiltinRenderTargets();		//ビルトインレンダーターゲットの生成（ポストプロセス、ブラー、モーションブラーなど）
	return true;
}

void Engine::InitBindings(TextureManager* pTextureManager)
{
	this->m_pTextureManager = pTextureManager;	//テクスチャマネージャの保存
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
		assert(target.colorIndex < FRAME_BUFFER_COUNT);
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
		auto& rt = m_backBuffers[target.colorIndex];				// Get the back buffer render target for the specified index
		resource = rt.resource.Get();						// Get the resource for the back buffer
		rtvIndex = m_backBuffers[target.colorIndex].rtvIndex;
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

		const auto rtvHandle = m_pDescriptorHeapAllocator->GetRtvCpuHandle(rtvIndex);	// Get the RTV handle for the current render target slot

		m_pCommandList->OMSetRenderTargets(
			1,
			&rtvHandle,
			FALSE,
			nullptr
		);

		m_pCommandList->ClearRenderTargetView(
			rtvHandle,
			clearColor,
			0,
			nullptr
		);

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

		const auto rtvHandle = m_pDescriptorHeapAllocator->GetRtvCpuHandle(color.GetRtvIndex());	// Get the RTV handle for the current render target slot
		const auto dsvHandle = m_pDescriptorHeapAllocator->GetDsvCpuHandle(depth.GetDsvIndex());	// Get the DSV handle for the depth render target

		m_pCommandList->OMSetRenderTargets(
			1,
			&rtvHandle,
			FALSE,
			&dsvHandle
		);

		m_pCommandList->ClearRenderTargetView(
			rtvHandle,
			color.GetClearColor(),
			0,
			nullptr
		);

		m_pCommandList->ClearDepthStencilView(
			dsvHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f,
			0,
			0,
			nullptr
		);

		return;
	}
	else if (target.type == RenderPassTargetType::DepthOnly)
	{// Depth only uses depth buffer, and next state is DepthWrite
		auto& depth = *m_builtinRenderTargets.at(target.depthIndex);

		depth.TransitionToState(m_pCommandList.Get(), GpuTexture::ResourceState::DepthWrite);

		SetViewPortAndScissorRect(depth);	// Set the viewport and scissor rectangle for rendering

		const auto dsvHandle = m_pDescriptorHeapAllocator->GetDsvCpuHandle(depth.GetDsvIndex());	// Get the DSV handle for the depth-only render target

		m_pCommandList->OMSetRenderTargets(
			0,
			nullptr,
			FALSE,
			&dsvHandle
		);

		m_pCommandList->ClearDepthStencilView(
			dsvHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f,
			0,
			0,
			nullptr
		);

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
		assert(target.colorIndex < FRAME_BUFFER_COUNT);
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
		auto& rt = m_backBuffers[target.colorIndex];		// Get the back buffer render target for the specified index
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
	m_pCommandAllocator[m_currentBackBufferIndex]->Reset();		// Reset the command allocator for the current back buffer index
	m_pCommandList->Reset(										// Reset the command list
		m_pCommandAllocator[m_currentBackBufferIndex].Get(),	// Get the command allocator for the current back buffer index
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
	m_pSwapChain->Present(1, 0);	// Present with vertical sync

	// Wait for the previous frame to finish
	WaitRender();

	// Get the next back buffer index
	m_currentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}

bool Engine::ResizeSceneRenderTargets(UINT width, UINT height)
{
	if (width == 0 || height == 0) return false;	// Invalid size, return false

	// Get the scene render targets (color and depth)
	auto* sceneColor = GetBuiltinRenderTarget(BuiltinRenderTarget::SceneColor);
	auto* sceneDepth = GetBuiltinRenderTarget(BuiltinRenderTarget::SceneDepth);

	if (!sceneColor || !sceneDepth)
	{
		assert(false && "Scene render targets are not initialized");
		return false;
	}

	// Skip resizing to the same size
	if (sceneColor->GetWidth() == width &&
		sceneColor->GetHeight() == height &&
		sceneDepth->GetWidth() == width &&
		sceneDepth->GetHeight() == height)
	{
		return true;
	}

	// Wait for the GPU to finish rendering before resizing
	WaitRender();

	// Resize the scene render targets (color and depth)
	const bool colorResult = sceneColor->Resize(m_pDevice.Get(), m_pDescriptorHeapAllocator.get(), width, height);
	if (!colorResult)
	{
		assert(false && "SceneColor resize failed");
		return false;
	}

	const bool depthResult = sceneDepth->Resize(m_pDevice.Get(), m_pDescriptorHeapAllocator.get(), width, height);
	if (!depthResult)
	{
		assert(false && "SceneDepth resize failed after SceneColor resize");
		return false;
	}

	// Ensure that the resized render targets have the same dimensions
	assert(sceneColor->GetWidth() == sceneDepth->GetWidth());
	assert(sceneColor->GetHeight() == sceneDepth->GetHeight());

	return true;
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
	m_pDescriptorHeapAllocator = std::make_unique<DescriptorHeapAllocator>(m_pDevice.Get());
	m_pDescriptorHeapAllocator->Initialize();
}

//コマンドオブジェクトの生成
void Engine::CreateCommandObjects()
{
	for (size_t i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		//コマンドアロケーターの生成
		result = m_pDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,			//直接コマンド
			IID_PPV_ARGS(&m_pCommandAllocator[i])	//コマンドアロケーターのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
		);
	}

	result = m_pDevice->CreateCommandList(
		0,														//ノードマスク
		D3D12_COMMAND_LIST_TYPE_DIRECT,							//直接コマンド
		m_pCommandAllocator[m_currentBackBufferIndex].Get(),	//コマンドアロケーター
		nullptr,												//パイプラインステートオブジェクト
		IID_PPV_ARGS(&m_pCommandList)							//コマンドリストのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

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
}

//スワップチェーンの生成
void Engine::CreateSwapChain()
{
	//DXGIファクトリの生成
	IDXGIFactory4* pDXGIFactory4 = nullptr;						//DXGIファクトリ6
	result = CreateDXGIFactory1(IID_PPV_ARGS(&pDXGIFactory4));	//DXGIファクトリ1の生成

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};	//スワップチェーンの設定構造体

	swapchainDesc.Width = m_frameBufferWidth;					//ウィンドウの幅
	swapchainDesc.Height = m_frameBufferHeight;					//ウィンドウの高さ
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			//色フォーマット
	swapchainDesc.Stereo = false;								//ステレオ表示かどうか
	swapchainDesc.SampleDesc.Count = 1;							//マルチサンプリングしない
	swapchainDesc.SampleDesc.Quality = 0;						//クオリティレベル0
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;			//バックバッファとして使用
	swapchainDesc.BufferCount = 2;								//バッファ数(ダブルバッファリング)
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;				//ウィンドウサイズに合わせて伸縮
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	//フリップ後破棄
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;		//アルファモードは指定しない
	swapchainDesc.Flags = 0;									//特に指定なし

	//スワップチェーンの生成
	IDXGISwapChain1* pSwapChain = nullptr;

	result = pDXGIFactory4->CreateSwapChainForHwnd(
		m_pCommandQueue.Get(),			//コマンドキュー
		hwnd,							//ウィンドウハンドル
		&swapchainDesc,					//スワップチェーンの設定構造体
		nullptr,						//フルスクリーン設定(nullptrでデフォルト)
		nullptr,						//制限出力(nullptrで制限なし)
		&pSwapChain						//スワップチェーンのアドレスを取得
	);

	//IDXGISwapChain4に変換
	result = pSwapChain->QueryInterface(IID_PPV_ARGS(m_pSwapChain.ReleaseAndGetAddressOf()));

	//バックバッファのインデックスを取得
	m_currentBackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

	//不要になったリソースを解放
	pSwapChain->Release();
	pDXGIFactory4->Release();
}

void Engine::CreateFence()
{
	// Create a fence for GPU synchronization
	result = m_pDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pFence)
	);

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

void Engine::CreateBackBuffers()
{
	// Get swap chain description
	DXGI_SWAP_CHAIN_DESC swcDesc = {};	// Swap chain description structure
	result = m_pSwapChain->GetDesc(&swcDesc);

	for (UINT idx = 0; idx < swcDesc.BufferCount; idx++)
	{
		auto& buffer = m_backBuffers[idx];
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };	// Back buffer clear color (black)

		// Get render target view handle
		auto rtvIndex = m_pDescriptorHeapAllocator->AllocateRtv();				// Allocate render target view index
		auto rtvHandle = m_pDescriptorHeapAllocator->GetRtvCpuHandle(rtvIndex);	// Get render target view handle

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
		m_pDevice->CreateRenderTargetView(
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

	sceneDepth->Initialize(m_pDevice.Get(), m_pDescriptorHeapAllocator.get(), desc);
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
	rt->Initialize(m_pDevice.Get(), m_pDescriptorHeapAllocator.get(), desc);
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
	rt->Initialize(m_pDevice.Get(), m_pDescriptorHeapAllocator.get(), desc);
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