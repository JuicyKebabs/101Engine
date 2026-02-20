#include "Engine.h"

using namespace DirectX;
using namespace std;

//コンストラクタ
Engine::Engine()
{
}

//デストラクタ
Engine::~Engine()
{
}

//初期化
bool Engine::Initialize(HWND hwnd, UINT m_FrameBufferWidth, UINT m_FrameBufferHeight)
{
	this->hwnd = hwnd;									//ウィンドウハンドルの保存
	this->m_FrameBufferWidth = m_FrameBufferWidth;		//フレームバッファの幅の保存
	this->m_FrameBufferHeight = m_FrameBufferHeight;	//フレームバッファの高さの保存

	CreateDevice();			//デバイスの生成
	CreateCommandObjects();	//コマンドオブジェクトの生成
	CreateSwapChain();		//スワップチェーンの生成
	CreateFence();			//フェンスの生成
	CreateViewport();		//ビューポートの生成
	CreateScissorRect();	//シザー矩形の生成
	CreateRenderTarget();	//レンダーターゲットの生成
	CreateDepthStencil();	//深度ステンシルの生成
	return true;
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

//描画開始
void Engine::RenderBegin()
{
	// 現在のレンダーターゲットを更新
	m_currentRenderTarget = m_pRenderTargets[m_currentBackBufferIndex].Get();

	// コマンドを初期化してためる準備をする
	m_pCommandAllocator[m_currentBackBufferIndex]->Reset();	//コマンドアロケータのリセット
	m_pCommandList->Reset(									//コマンドリストのリセット
		m_pCommandAllocator[m_currentBackBufferIndex].Get(),	//使用するコマンドアロケータ
		nullptr													//パイプラインステートオブジェクト(nullptrで未設定)
	);

	// ビューポートとシザー矩形を設定
	m_pCommandList->RSSetViewports(1, &m_viewport);
	m_pCommandList->RSSetScissorRects(1, &m_scissorRect);

	//各ヒープのハンドルを取得
	auto rtvHandle = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();		//レンダーターゲットビューの先頭ハンドル
	auto dsvH = m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();			//デプスステンシルビューの先頭ハンドル

	//バックバッファのインデックス分だけレンダーターゲットビューのハンドルを進める
	rtvHandle.ptr += m_currentBackBufferIndex * m_rtvDescriptorSize;

	// レンダーターゲットが使用可能になるまで待つ
	auto barrier = 
		CD3DX12_RESOURCE_BARRIER::Transition(
			m_currentRenderTarget,				//現在のレンダーターゲット
			D3D12_RESOURCE_STATE_PRESENT,		//現在の状態
			D3D12_RESOURCE_STATE_RENDER_TARGET	//遷移後の状態
		);

	//バリアの設定コマンド
	m_pCommandList->ResourceBarrier(1, &barrier);

	//レンダーターゲットとデプスステンシルビューの設定
	m_pCommandList->OMSetRenderTargets(
		1,				//レンダーターゲットの数
		&rtvHandle,		//レンダーターゲットビューのハンドル
		true,			//レンダーターゲットの配列が連続しているかどうか
		&dsvH			//デプスステンシルビューのハンドル
	);

	//画面クリア
	float clearColor[] = { 0.2f, 0.2f, 1.0f, 1.0f };	//クリアカラー(RGBA)
	//レンダーターゲットのクリア
	m_pCommandList->ClearRenderTargetView(
		rtvHandle,		//クリアするレンダーターゲットビューのハンドル
		clearColor,		//クリアカラー
		0,				//クリアする矩形の数
		nullptr			//クリアする矩形(NULLで全画面)
	);

	//深度バッファのクリア
	m_pCommandList->ClearDepthStencilView(
		dsvH,					//デプスステンシルビューのハンドル
		D3D12_CLEAR_FLAG_DEPTH,	//クリア先指定(深度バッファ)
		1.0f,					//デプスクリア値(ビューボリュームの最大値で全クリア)
		0,						//ステンシルクリア値未使用なのでクリアなし()
		0,						//クリア範囲配列のサイズ(なし)
		nullptr					//クリア範囲配列(なし)
	);
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

//描画終了
void Engine::RenderEnd()
{
	// レンダーターゲットに書き込み終わるまで待つ
	//バリアの設定
	auto barrier = 
		CD3DX12_RESOURCE_BARRIER::Transition(
			m_currentRenderTarget,				//現在のレンダーターゲット
			D3D12_RESOURCE_STATE_RENDER_TARGET, //現在の状態
			D3D12_RESOURCE_STATE_PRESENT		//遷移後の状態
		);

	//バリアの設定コマンド
	m_pCommandList->ResourceBarrier(1, &barrier);

	//コマンドリストのクローズ
	m_pCommandList->Close();

	//コマンドリストの実行
	ID3D12CommandList* cmdLists[] = { m_pCommandList.Get() };	//実行するコマンドリストの配列
	m_pCommandQueue->ExecuteCommandLists(
		1,			//コマンドリストの数
		cmdLists	//コマンドリスト配列の先頭アドレス
	);

	//バックバッファの入れ替え
	m_pSwapChain->Present(1, 0);	//垂直同期ありで表示

	//前のフレームの終了待ち
	WaitRender();

	//次のバックバッファインデックスを取得
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
void Engine::CreateRenderTarget()
{
	//レンダーターゲット用デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};			//ディスクリプタヒープの設定構造体
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		//ビューの種類(レンダーターゲットビュー用)
	heapDesc.NodeMask = 0;								//GPU識別用のノードマスク
	heapDesc.NumDescriptors = 2;						//ディスクリプタ数(ダブルバッファリングなので2つ)
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	//特に指定なし

	//デスクリプタヒープの生成
	result = m_pDevice->CreateDescriptorHeap(
		&heapDesc,					//デスクリプタヒープの設定構造体
		IID_PPV_ARGS(&m_pRTVHeap)	//デスクリプタヒープのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	//RTVデスクリプタサイズの取得
	m_rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();

	//レンダーターゲットビュー(RTV)の生成
	//レンダーターゲットビュー設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};	//レンダーターゲットビューの設定構造体

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			//色フォーマット
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;	//2Dテクスチャ

	//スワップチェーンの設定取得
	DXGI_SWAP_CHAIN_DESC swcDesc = {};	//スワップチェーンの設定構造体
	result = m_pSwapChain->GetDesc(&swcDesc);

	//スワップチェーンのバッファ数分だけID3D12Resourceのポインタを格納する配列を生成
	std::vector<ID3D12Resource*> backBuffers(swcDesc.BufferCount);

	//バッファの数だけループ
	for (int idx = 0; idx < swcDesc.BufferCount; idx++)
	{
		//スワップチェーンからバッファを取得
		result = m_pSwapChain->GetBuffer(
			idx,															//取得するバッファのインデックス
			IID_PPV_ARGS(m_pRenderTargets[idx].ReleaseAndGetAddressOf())	//レンダーターゲットのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
		);

		//レンダーターゲットビューの生成
		m_pDevice->CreateRenderTargetView(
			m_pRenderTargets[idx].Get(),	//レンダーターゲットに設定するバッファ
			&rtvDesc,						//レンダーターゲットビューの設定(sRGB用設定)
			rtvHandle						//レンダーターゲットビューを格納するディスクリプタヒープのハンドル
		);

		//デスクリプタヒープの要素数分だけポインタを進める
		rtvHandle.ptr += m_rtvDescriptorSize;
	}
}

//深度ステンシルの生成
void Engine::CreateDepthStencil()
{
	//デスクリプタヒープの生成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};	//設定構造体

	dsvHeapDesc.NumDescriptors = 1;							//深度ビュー1つ分
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;		//デプスステンシルビューとして使用
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	//特に指定なし

	result = m_pDevice->CreateDescriptorHeap(	//生成
		&dsvHeapDesc,				//デスクリプタヒープの設定構造体
		IID_PPV_ARGS(&m_pDsvHeap)	//デスクリプタヒープのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	//デスクリプタサイズの取得
	m_dsvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV	//デプスステンシルビューを指定
	);

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

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = 
		m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();	//デスクリプタヒープの先頭ハンドルを取得

	m_pDevice->CreateDepthStencilView(	//生成
		m_pDepthStencilBuffer.Get(),	//深度ステンシルバッファ
		&dsvDesc,						//デプスステンシルビューの設定構造体
		dsvHandle						//デスクリプタヒープのハンドル
	);
}