#include "Engine/Engine.h"
#include "Engine/Resource/TextureManager.h"

using namespace DirectX;
using namespace std;

bool Engine::InitCore(HWND hwnd, UINT m_FrameBufferWidth, UINT m_FrameBufferHeight)
{
	this->hwnd = hwnd;									//ウィンドウハンドルの保存
	this->m_FrameBufferWidth = m_FrameBufferWidth;		//フレームバッファの幅の保存
	this->m_FrameBufferHeight = m_FrameBufferHeight;	//フレームバッファの高さの保存

	CreateDevice();						//デバイスの生成
	CreateDescriptorHeapAllocator();	//ディスクリプタヒープアロケータの生成
	CreateCommandObjects();				//コマンドオブジェクトの生成
	CreateSwapChain();					//スワップチェーンの生成
	CreateFence();						//フェンスの生成
	CreateViewport();					//ビューポートの生成
	CreateScissorRect();				//シザー矩形の生成
	CreateBackBuffers();				//バックバッファの生成
	CreateBuiltinRenderTargets();		//ビルトインレンダーターゲットの生成（ポストプロセス、ブラー、モーションブラーなど）
	CreateDepthStencil();				//深度ステンシルの生成
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
	// Check if the target is valid
	if (target.type != RenderPassTargetType::BackBuffer && target.type != RenderPassTargetType::Builtin)
	{
		assert(false && "Invalid render pass target type");
	}
	else if (target.index >= (target.type == RenderPassTargetType::BackBuffer ? FRAME_BUFFER_COUNT : static_cast<size_t>(BuiltinRenderTarget::Count)))
	{
		assert(false && "Invalid render pass target index");
	}

	ID3D12Resource* resource = nullptr;
	uint32_t rtvIndex = 0;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES nextState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	if(target.type == RenderPassTargetType::BackBuffer)
	{
		auto& rt = m_backBuffers[target.index];				// Get the back buffer render target for the specified index
		resource = rt.resource.Get();						// Get the resource for the back buffer
		rtvIndex = m_backBuffers[target.index].rtvIndex;
		currentState = rt.currentState;						// Get the current resource state of the back buffer
		clearColor[0] = rt.clearColor[0];
		clearColor[1] = rt.clearColor[1];
		clearColor[2] = rt.clearColor[2];
		clearColor[3] = rt.clearColor[3];

		rt.currentState = GpuTexture::ConvertToD3D12State(GpuTexture::ResourceState::Write);	// Update the current state of the back buffer to "Write" for rendering
	}
	else if (target.type == RenderPassTargetType::Builtin)
	{
		auto rt = m_builtinRenderTargets[static_cast<size_t>(target.index)].get();	// Get the render target for the specified built-in index
		resource = rt->GetResource();												// Get the resource for the built-in render target
		rtvIndex = rt->GetRtvIndex();												// Get the RTV index for the built-in render target
		currentState = GpuTexture::ConvertToD3D12State(rt->GetState());	// Get the current resource state of the built-in render target
		clearColor[0] = rt->GetClearColor()[0];
		clearColor[1] = rt->GetClearColor()[1];
		clearColor[2] = rt->GetClearColor()[2];
		clearColor[3] = rt->GetClearColor()[3];

		rt->MarkAsRenderTarget();	// Mark the built-in render target as being written to (rendering)
	}

	// Set up the resource barrier to render target state
	auto barrier =
		CD3DX12_RESOURCE_BARRIER::Transition(
			resource,		// Current render target resource
			currentState,	// Current resource state
			nextState		// New resource state for rendering
		);

	// Set the resource barrier command
	m_pCommandList->ResourceBarrier(1, &barrier);

	auto dsvHandle = m_pDescriptorHeapAllocator->GetDsvCpuHandle(0);		// Get the DSV handle (assuming a single depth stencil view for simplicity)
	auto rtvHandle = m_pDescriptorHeapAllocator->GetRtvCpuHandle(rtvIndex);	// Get the RTV handle for the current render target slot

	// Set the render target and depth stencil view
	m_pCommandList->OMSetRenderTargets(
		1,				// Number of render targets
		&rtvHandle,		// Render target view handle
		true,			// Whether the render target array is contiguous
		&dsvHandle		// Depth stencil view handle
	);

	// Clear the render target
	m_pCommandList->ClearRenderTargetView(
		rtvHandle,					// Handle to the render target view to clear
		clearColor,					// Clear color
		0,							// Number of rectangles to clear
		nullptr						// Clear rectangle (NULL means full screen)
	);

	// Clear the depth buffer
	m_pCommandList->ClearDepthStencilView(
		dsvHandle,				// Handle to the depth stencil view
		D3D12_CLEAR_FLAG_DEPTH,	// Clear target specification (depth buffer)
		1.0f,					// Depth clear value (clear to maximum value of view volume)
		0,						// Stencil clear value not used, so no clear
		0,						// Size of clear area array (none)
		nullptr					// Clear area array (none)
	);
}

// End rendering to the render target
void Engine::EndPass(RenderPassTarget target)
{
	// Check if the target is valid
	if(target.type != RenderPassTargetType::BackBuffer && target.type != RenderPassTargetType::Builtin)
	{
		assert(false && "Invalid render pass target type");
	}
	else if(target.index >= (target.type == RenderPassTargetType::BackBuffer ? FRAME_BUFFER_COUNT : static_cast<size_t>(BuiltinRenderTarget::Count)))
	{
		assert(false && "Invalid render pass target index");
	}

	ID3D12Resource* resource = nullptr;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_STATES nextState = D3D12_RESOURCE_STATE_COMMON;

	if(target.type == RenderPassTargetType::BackBuffer)
	{
		auto& rt = m_backBuffers[target.index];				// Get the back buffer render target for the specified index
		resource = rt.resource.Get();						// Get the resource for the back buffer
		currentState = rt.currentState;						// Get the current resource state of the back buffer
		nextState = D3D12_RESOURCE_STATE_PRESENT;			// Next state for the back buffer is "Present" for presentation to the screen
		rt.currentState = D3D12_RESOURCE_STATE_PRESENT;		// Update the current state of the back buffer to "Present" for post-processing
	}
	else if (target.type == RenderPassTargetType::Builtin)
	{
		auto rt = m_builtinRenderTargets[static_cast<size_t>(target.index)].get();				// Get the render target for the specified built-in index
		resource = rt->GetResource();															// Get the resource for the built-in render target
		currentState = GpuTexture::ConvertToD3D12State(rt->GetState());				// Get the current resource state of the built-in render target
		nextState = GpuTexture::ConvertToD3D12State(GpuTexture::ResourceState::Read);	// Next state for the built-in render target is "Read" for post-processing
		rt->MarkAsShaderResource();																		// Mark the built-in render target as being read from (post-processing)
	}

	// Set up the resource barrier to transition to the next state
	auto barrier =
		CD3DX12_RESOURCE_BARRIER::Transition(
			resource,		// Current render target
			currentState,	// Current resource state
			nextState		// New resource state for post-processing
		);

	// Set the resource barrier command
	m_pCommandList->ResourceBarrier(1, &barrier);
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

	// Set up the viewport and scissor rectangle for rendering
	m_pCommandList->RSSetViewports(1, &m_viewport);
	m_pCommandList->RSSetScissorRects(1, &m_scissorRect);
}

//前のフレームの終了待ち
void Engine::WaitRender()
{
	//描画終了待ち
	const UINT64 fenceValue = m_fenceValue[m_currentBackBufferIndex];
	m_pCommandQueue->Signal(m_pFence.Get(), fenceValue);
	m_fenceValue[m_currentBackBufferIndex]++;

	// 次のフレームの描画準備がまだであれば待機する.
	if (m_pFence->GetCompletedValue() < fenceValue)
	{
		// 完了時にイベントを設定.
		auto hr = m_pFence->SetEventOnCompletion(fenceValue, m_fenceEvent);
		if (FAILED(hr))
		{
			return;
		}

		// 待機処理.
		if (WAIT_OBJECT_0 != WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE))
		{
			return;
		}
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
	m_pDescriptorHeapAllocator = make_unique<DescriptorHeapAllocator>(m_pDevice.Get());
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

	swapchainDesc.Width = m_FrameBufferWidth;					//ウィンドウの幅
	swapchainDesc.Height = m_FrameBufferHeight;					//ウィンドウの高さ
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

//フェンスの生成
void Engine::CreateFence()
{
	for (int i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		//フェンス値を初期化
		m_fenceValue[i] = 0;
	}

	//フェンスの生成
	result = m_pDevice->CreateFence(
		0,						//初期化値
		D3D12_FENCE_FLAG_NONE,	//フラグ
		IID_PPV_ARGS(&m_pFence)	//フェンスのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	//同期を行うときのイベントハンドラを作成する。
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

//ビューポートの生成
void Engine::CreateViewport()
{
	//ビューポートの設定
	D3D12_VIEWPORT viewport = {};	//ビューポートの設定構造体

	viewport.Width = 
		static_cast<float>(m_FrameBufferWidth);		//出力の幅
	viewport.Height = 
		static_cast<float>(m_FrameBufferHeight);	//出力の高さ

	viewport.TopLeftX = 0;		//出力の左上X座標
	viewport.TopLeftY = 0;		//出力の左上Y座標
	viewport.MaxDepth = 1.0f;	//最大深度
	viewport.MinDepth = 0.0f;	//最小深度

	m_viewport = viewport;		//メンバ変数に保存
}

//シザー矩形の生成
void Engine::CreateScissorRect()
{
	//シザー矩形の設定
	D3D12_RECT scissorRect = {};	//シザー矩形の設定構造体

	scissorRect.top = 0;						//切り抜き上端座標
	scissorRect.left = 0;						//切り抜き左端座標
	scissorRect.right = m_FrameBufferWidth;		//切り抜き右端座標
	scissorRect.bottom = m_FrameBufferHeight;	//切り抜き下端座標

	m_scissorRect = scissorRect;	//メンバ変数に保存
}

//レンダーターゲットの生成
void Engine::CreateBackBuffers()
{
	//スワップチェーンの設定取得
	DXGI_SWAP_CHAIN_DESC swcDesc = {};	//スワップチェーンの設定構造体
	result = m_pSwapChain->GetDesc(&swcDesc);

	//バッファの数だけループ
	for (int idx = 0; idx < swcDesc.BufferCount; idx++)
	{
		auto& buffer = m_backBuffers[idx];
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };	//バックバッファのクリアカラー(黒)

		//レンダーターゲットビューのハンドルを取得
		auto rtvIndex = m_pDescriptorHeapAllocator->AllocateRtv();				//レンダーターゲットビューのインデックスを割り当て
		auto rtvHandle = m_pDescriptorHeapAllocator->GetRtvCpuHandle(rtvIndex);	//レンダーターゲットビューのハンドルを取得

		//レンダーターゲットビュー(RTV)の生成
		//レンダーターゲットビュー設定
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};	//レンダーターゲットビューの設定構造体

		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			//色フォーマット
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;	//2Dテクスチャ

		//スワップチェーンからバッファを取得
		result = m_pSwapChain->GetBuffer(
			idx,															//取得するバッファのインデックス
			IID_PPV_ARGS(buffer.resource.ReleaseAndGetAddressOf())	//レンダーターゲットのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
		);

		//レンダーターゲットビューの生成
		m_pDevice->CreateRenderTargetView(
			buffer.resource.Get(),		//レンダーターゲットに設定するバッファ
			&rtvDesc,		//レンダーターゲットビューの設定(sRGB用設定)
			rtvHandle		//レンダーターゲットビューを格納するディスクリプタヒープのハンドル
		);

		buffer.rtvIndex = rtvIndex;								//バックバッファのRTVインデックスを保存
		buffer.currentState = D3D12_RESOURCE_STATE_COMMON;	//バックバッファの現在のリソース状態を保存
		buffer.clearColor[0] = clearColor[0];				//バックバッファのクリアカラーを保存
		buffer.clearColor[1] = clearColor[1];
		buffer.clearColor[2] = clearColor[2];
		buffer.clearColor[3] = clearColor[3];
	}
}

void Engine::CreateBuiltinRenderTargets()
{
	for(auto& target : m_builtinRenderTargets) {
		target = make_unique<GpuTexture>();
	}

	CreatePostProcessRenderTarget();
}

void Engine::CreateShadowMapRenderTarget()
{
	// Clear color
	float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R16G16B16A16_FLOAT, clearColor);

	// Heap properties
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// Resource description
	auto resourceDesc = m_backBuffers[0].resource->GetDesc();

	// Format 
	GpuTexture::Format format = GpuTexture::Format::RGBA16F;
	resourceDesc.Format = GpuTexture::ConvertToDXGIFormat(format);

	// Initial state
	GpuTexture::ResourceState initialState = GpuTexture::ResourceState::Read;
	auto d3dState = GpuTexture::ConvertToD3D12State(initialState);

	// Create the render target resource
	ComPtr<ID3D12Resource> renderTarget;
	result = m_pDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		d3dState,
		&clearValue,
		IID_PPV_ARGS(renderTarget.ReleaseAndGetAddressOf())
	);

	// Create render target view (RTV)
	auto rtvIndex = m_pDescriptorHeapAllocator->AllocateRtv();				// Allocate an RTV descriptor from the heap allocator
	auto rtvHandle = m_pDescriptorHeapAllocator->GetRtvCpuHandle(rtvIndex);	// Get the CPU handle for the allocated RTV descriptor
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	m_pDevice->CreateRenderTargetView(
		renderTarget.Get(),
		&rtvDesc,
		rtvHandle
	);

}

// Create post-process render target
void Engine::CreatePostProcessRenderTarget()
{
	// Clear color
	float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R16G16B16A16_FLOAT, clearColor);

	// Heap properties
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// Resource description
	auto resourceDesc = m_backBuffers[0].resource->GetDesc();

	// Format 
	GpuTexture::Format format = GpuTexture::Format::RGBA16F;
	resourceDesc.Format = GpuTexture::ConvertToDXGIFormat(format);

	// Initial state
	GpuTexture::ResourceState initialState = GpuTexture::ResourceState::Read;
	auto d3dState = GpuTexture::ConvertToD3D12State(initialState);

	// Create the render target resource
	ComPtr<ID3D12Resource> renderTarget;
	result = m_pDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		d3dState,
		&clearValue,
		IID_PPV_ARGS(renderTarget.ReleaseAndGetAddressOf())
		);

	// Create render target view (RTV)
	auto rtvIndex = m_pDescriptorHeapAllocator->AllocateRtv();				// Allocate an RTV descriptor from the heap allocator
	auto rtvHandle = m_pDescriptorHeapAllocator->GetRtvCpuHandle(rtvIndex);	// Get the CPU handle for the allocated RTV descriptor
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	m_pDevice->CreateRenderTargetView(
		renderTarget.Get(),
		&rtvDesc,
		rtvHandle
	);

	// Create shader resource view (SRV)
	auto srvIndex = m_pDescriptorHeapAllocator->AllocateCbvSrvUav();				// Allocate an SRV descriptor from the heap allocator
	auto srvHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavCpuHandle(srvIndex);	// Get the CPU handle for the allocated SRV descriptor
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	m_pDevice->CreateShaderResourceView(
		renderTarget.Get(),
		&srvDesc,
		srvHandle
	);

	// Create the RenderTargetTexture object and initialize it
	GpuTexture::InitDesc desc = {};
	desc.clearColor[0] = clearColor[0];
	desc.clearColor[1] = clearColor[1];
	desc.clearColor[2] = clearColor[2];
	desc.clearColor[3] = clearColor[3];
	desc.format = format;
	desc.height = m_FrameBufferHeight;
	desc.width = m_FrameBufferWidth;
	desc.m_rtvIndex = rtvIndex;
	desc.m_srvIndex = srvIndex;
	desc.initialState = initialState;
	m_builtinRenderTargets[static_cast<size_t>(BuiltinRenderTarget::SceneColor)]->Initialize(renderTarget, desc);
}

//深度ステンシルの生成
void Engine::CreateDepthStencil()
{
	//深度用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp =
		CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);	//ヒーププロパティ

	//深度バッファの設定
	D3D12_RESOURCE_DESC depthResDesc = {};	//深度バッファの設定構造体

	depthResDesc.Dimension
		= D3D12_RESOURCE_DIMENSION_TEXTURE2D;			//２次元テクスチャデータ
	depthResDesc.Width = m_FrameBufferWidth;			//幅(レンダーターゲットと同じ)
	depthResDesc.Height = m_FrameBufferHeight;			//高さ(レンダーターゲットと同じ)
	depthResDesc.DepthOrArraySize = 1;					//深さ又は配列サイズ
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;		//深度値書き込み用フォーマット
	depthResDesc.SampleDesc.Count = 1;					//１ピクセル当たり１つのサンプル
	depthResDesc.Flags
		= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;		//深度ステンシルとして使用
	depthResDesc.MipLevels = 1;							//ミップレベル
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	//レイアウト
	depthResDesc.Alignment = 0;							//アライメント

	//クリアバリュー
	D3D12_CLEAR_VALUE depthClearValue = {};	//クリアバリュー

	depthClearValue.DepthStencil.Depth = 1.0f;		//深さ１(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;	//32bit深度値としてクリア

	//深度ステンシルバッファの生成
	result = m_pDevice->CreateCommittedResource(
		&depthHeapProp,							//ヒーププロパティ
		D3D12_HEAP_FLAG_NONE,					//ヒープフラグ
		&depthResDesc,							//リソース設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,		//深度書き込み可能状態で生成
		&depthClearValue,						//クリアバリュー
		IID_PPV_ARGS(&m_pDepthStencilBuffer)	//深度ステンシルバッファのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	//デプスステンシルビューの生成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};	//設定構造体

	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;					//デプス値32bit
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	//2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;					//フラグなし

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pDescriptorHeapAllocator->GetDsvCpuHandle(0);

	m_pDevice->CreateDepthStencilView(	//生成
		m_pDepthStencilBuffer.Get(),	//深度ステンシルバッファ
		&dsvDesc,						//デプスステンシルビューの設定構造体
		dsvHandle						//デスクリプタヒープのハンドル
	);
}