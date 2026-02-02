#include "Renderer.h"
#include <algorithm>
#include <vector>
#include "Engine.h"
#include "App.h"
#include "d3dx12.h"
#include "SharedStruct.h"
#include "AssimpLoader.h"

using namespace DirectX;


//デストラクタ
Renderer::~Renderer()
{
	//ルートシグネチャの解放
	if (m_pRootSignature)
	{
		delete m_pRootSignature;
		m_pRootSignature = nullptr;
	}
	//パイプラインステートの解放
	for (auto& pPipelineState : m_pPipelineStateWorldLight)
	{
		if (pPipelineState)
		{
			delete pPipelineState;
			pPipelineState = nullptr;
		}
	}
	for (auto& pPipelineState : m_pPipelineStateWorldNoLight)
	{
		if (pPipelineState)
		{
			delete pPipelineState;
			pPipelineState = nullptr;
		}
	}
	for (auto& pPipelineState : m_pPipelineStateEffect)
	{
		if (pPipelineState)
		{
			delete pPipelineState;
			pPipelineState = nullptr;
		}
	}
	for (auto& pPipelineState : m_pPipelineStateScreen)
	{
		if (pPipelineState)
		{
			delete pPipelineState;
			pPipelineState = nullptr;
		}
	}
	//オブジェクト用定数バッファの解放
	for (int i = 0; i < Engine::FRAME_BUFFER_COUNT; i++)
	{
		for (auto& pCB : m_objectCBWorld[i])
		{
			if (pCB)
			{
				delete pCB;
				pCB = nullptr;
			}
		}
		m_objectCBWorld[i].clear();
	}
}

//初期化
void Renderer::Initialize(ID3D12Device* pDevice, CameraInfo* pInfo)
{
	m_pDevice = pDevice;	//デバイスの保存
	m_cameraInfo = pInfo;	//カメラ情報構造体の保存
	m_directionalLight = {}; //初期化

	//ルートシグネチャの生成
	m_pRootSignature = new RootSignature(m_pDevice);

	//パイプラインステートの生成
	for (auto& pPipelineState : m_pPipelineStateWorldNoLight)
	{
		pPipelineState = new PipelineState(m_pDevice);
		pPipelineState->SetInputLayout(Vertex::InputLayout);						//入力レイアウトの設定
		pPipelineState->SetRootSignature(m_pRootSignature->GetRootSignature());	//ルートシグネチャの設定
		pPipelineState->SetVertexShader(L"VertexShader.hlsl");					//頂点シェーダーの設定
	}
	for (auto& pPipelineState : m_pPipelineStateWorldLight)
	{
		pPipelineState = new PipelineState(m_pDevice);
		pPipelineState->SetInputLayout(Vertex::InputLayout);						//入力レイアウトの設定
		pPipelineState->SetRootSignature(m_pRootSignature->GetRootSignature());	//ルートシグネチャの設定
		pPipelineState->SetVertexShader(L"VertexShader.hlsl");					//頂点シェーダーの設定
	}
	for (auto& pPipelineState : m_pPipelineStateEffect)
	{
		pPipelineState = new PipelineState(m_pDevice);
		pPipelineState->SetInputLayout(Vertex::InputLayout);					//入力レイアウトの設定
		pPipelineState->SetRootSignature(m_pRootSignature->GetRootSignature());	//ルートシグネチャの設定
		pPipelineState->SetVertexShader(L"VertexShader.hlsl", "BillboardVS");	//頂点シェーダーの設定
	}
	for (auto& pPipelineState : m_pPipelineStateScreen)
	{
		pPipelineState = new PipelineState(m_pDevice);
		pPipelineState->SetInputLayout(Vertex::InputLayout);						//入力レイアウトの設定
		pPipelineState->SetRootSignature(m_pRootSignature->GetRootSignature());	//ルートシグネチャの設定
		pPipelineState->SetVertexShader(L"VertexShader.hlsl");					//頂点シェーダーの設定
	}

	//ワールド座標用パイプラインステートの設定
	//不透明設定
	m_pPipelineStateWorldNoLight[BLEND_OPAQUE]->SetPixelShader(L"PixelShader.hlsl", "BasicPS");	//ピクセルシェーダーの設定
	m_pPipelineStateWorldNoLight[BLEND_OPAQUE]->EnableAlphaBlend(false);							//不透明設定
	m_pPipelineStateWorldNoLight[BLEND_OPAQUE]->EnableDepthWrite(true);							//深度書き込み有効
	m_pPipelineStateWorldNoLight[BLEND_OPAQUE]->Create();											//生成
	//マスク設定
	m_pPipelineStateWorldNoLight[BLEND_MASKED]->SetPixelShader(L"PixelShader.hlsl", "BasicPSMasked");	//ピクセルシェーダーの設定
	m_pPipelineStateWorldNoLight[BLEND_MASKED]->EnableAlphaBlend(false);								//不透明設定
	m_pPipelineStateWorldNoLight[BLEND_MASKED]->EnableDepthWrite(true);								//深度書き込み有効
	m_pPipelineStateWorldNoLight[BLEND_MASKED]->Create();												//生成
	//透明設定
	m_pPipelineStateWorldNoLight[BLEND_TRANSPARENT]->SetPixelShader(L"PixelShader.hlsl", "BasicPS");	//ピクセルシェーダーの設定
	m_pPipelineStateWorldNoLight[BLEND_TRANSPARENT]->EnableAlphaBlend(true);							//透明設定
	m_pPipelineStateWorldNoLight[BLEND_TRANSPARENT]->EnableDepthWrite(false);							//深度書き込み無効
	m_pPipelineStateWorldNoLight[BLEND_TRANSPARENT]->Create();											//生成

	//不透明設定
	m_pPipelineStateWorldLight[BLEND_OPAQUE]->SetPixelShader(L"PixelShader.hlsl", "BasicLightPS");	//ピクセルシェーダーの設定
	m_pPipelineStateWorldLight[BLEND_OPAQUE]->EnableAlphaBlend(false);							//不透明設定
	m_pPipelineStateWorldLight[BLEND_OPAQUE]->EnableDepthWrite(true);							//深度書き込み有効
	m_pPipelineStateWorldLight[BLEND_OPAQUE]->Create();											//生成
	//マスク設定
	m_pPipelineStateWorldLight[BLEND_MASKED]->SetPixelShader(L"PixelShader.hlsl", "BasicLightPSMasked");	//ピクセルシェーダーの設定
	m_pPipelineStateWorldLight[BLEND_MASKED]->EnableAlphaBlend(false);								//不透明設定
	m_pPipelineStateWorldLight[BLEND_MASKED]->EnableDepthWrite(true);								//深度書き込み有効
	m_pPipelineStateWorldLight[BLEND_MASKED]->Create();												//生成
	//透明設定
	m_pPipelineStateWorldLight[BLEND_TRANSPARENT]->SetPixelShader(L"PixelShader.hlsl", "BasicLightPS");	//ピクセルシェーダーの設定
	m_pPipelineStateWorldLight[BLEND_TRANSPARENT]->EnableAlphaBlend(true);							//透明設定
	m_pPipelineStateWorldLight[BLEND_TRANSPARENT]->EnableDepthWrite(false);							//深度書き込み無効
	m_pPipelineStateWorldLight[BLEND_TRANSPARENT]->Create();											//生成

	//エフェクト用パイプラインステートの設定
//不透明設定
	m_pPipelineStateEffect[BLEND_OPAQUE]->SetPixelShader(L"PixelShader.hlsl", "EffectPS");	//ピクセルシェーダーの設定
	m_pPipelineStateEffect[BLEND_OPAQUE]->EnableAlphaBlend(false);							//不透明設定
	m_pPipelineStateEffect[BLEND_OPAQUE]->EnableDepthWrite(true);							//深度書き込み有効
	m_pPipelineStateEffect[BLEND_OPAQUE]->EnableDepthTest(true);							//深度テスト無効
	m_pPipelineStateEffect[BLEND_OPAQUE]->SetCullMode(D3D12_CULL_MODE_NONE);				//カリング無効化
	m_pPipelineStateEffect[BLEND_OPAQUE]->Create();											//生成
	//マスク設定
	m_pPipelineStateEffect[BLEND_MASKED]->SetPixelShader(L"PixelShader.hlsl", "EffectPSMasked");	//ピクセルシェーダーの設定
	m_pPipelineStateEffect[BLEND_MASKED]->EnableAlphaBlend(false);									//不透明設定
	m_pPipelineStateEffect[BLEND_MASKED]->EnableDepthWrite(true);									//深度書き込み有効
	m_pPipelineStateEffect[BLEND_MASKED]->EnableDepthTest(true);									//深度テスト無効
	m_pPipelineStateEffect[BLEND_MASKED]->SetCullMode(D3D12_CULL_MODE_NONE);						//カリング無効化
	m_pPipelineStateEffect[BLEND_MASKED]->Create();													//生成
	//透明設定
	m_pPipelineStateEffect[BLEND_TRANSPARENT]->SetPixelShader(L"PixelShader.hlsl", "EffectPS");		//ピクセルシェーダーの設定
	m_pPipelineStateEffect[BLEND_TRANSPARENT]->EnableAlphaBlend(true);								//透明設定
	m_pPipelineStateEffect[BLEND_TRANSPARENT]->EnableDepthWrite(false);								//深度書き込み無効
	m_pPipelineStateEffect[BLEND_TRANSPARENT]->EnableDepthTest(true);								//深度テスト無効
	m_pPipelineStateEffect[BLEND_TRANSPARENT]->SetCullMode(D3D12_CULL_MODE_NONE);					//カリング無効化
	m_pPipelineStateEffect[BLEND_TRANSPARENT]->Create();											//生成

	//スクリーン座標用パイプラインステートの設定
	//不透明設定
	m_pPipelineStateScreen[BLEND_OPAQUE]->SetPixelShader(L"PixelShader.hlsl", "BasicScreenPS");	//ピクセルシェーダーの設定
	m_pPipelineStateScreen[BLEND_OPAQUE]->EnableAlphaBlend(false);							//不透明設定
	m_pPipelineStateScreen[BLEND_OPAQUE]->EnableDepthWrite(false);							//深度書き込み有効
	m_pPipelineStateScreen[BLEND_OPAQUE]->EnableDepthTest(false);							//深度テスト無効
	m_pPipelineStateScreen[BLEND_OPAQUE]->SetCullMode(D3D12_CULL_MODE_NONE);				//カリング無効化
	m_pPipelineStateScreen[BLEND_OPAQUE]->Create();											//生成
	//マスク設定
	m_pPipelineStateScreen[BLEND_MASKED]->SetPixelShader(L"PixelShader.hlsl", "BasicScreenPSMasked");	//ピクセルシェーダーの設定
	m_pPipelineStateScreen[BLEND_MASKED]->EnableAlphaBlend(false);								//不透明設定
	m_pPipelineStateScreen[BLEND_MASKED]->EnableDepthWrite(false);								//深度書き込み有効
	m_pPipelineStateScreen[BLEND_MASKED]->EnableDepthTest(false);								//深度テスト無効
	m_pPipelineStateScreen[BLEND_MASKED]->SetCullMode(D3D12_CULL_MODE_NONE);					//カリング無効化
	m_pPipelineStateScreen[BLEND_MASKED]->Create();												//生成
	//透明設定
	m_pPipelineStateScreen[BLEND_TRANSPARENT]->SetPixelShader(L"PixelShader.hlsl", "BasicScreenPS");		//ピクセルシェーダーの設定
	m_pPipelineStateScreen[BLEND_TRANSPARENT]->EnableAlphaBlend(true);								//透明設定
	m_pPipelineStateScreen[BLEND_TRANSPARENT]->EnableDepthWrite(false);								//深度書き込み無効
	m_pPipelineStateScreen[BLEND_TRANSPARENT]->EnableDepthTest(false);								//深度テスト無効
	m_pPipelineStateScreen[BLEND_TRANSPARENT]->SetCullMode(D3D12_CULL_MODE_NONE);					//カリング無効化
	m_pPipelineStateScreen[BLEND_TRANSPARENT]->Create();											//生成
}

//更新
void Renderer::Update(UINT currentBackBufferIndex, CameraInfo& info)
{
	//ワールドカメラ行列の更新
	m_worldView = DirectX::XMMatrixLookAtLH(
		XMVectorSet(info.position.x, info.position.y, info.position.z, 0.0f),	//カメラの位置
		XMVectorSet(info.target.x, info.target.y, info.target.z, 0.0f),			//カメラの注視点
		XMVectorSet(info.up.x, info.up.y, info.up.z, 0.0f));					//カメラの上方向ベクトル

	//ワールドプロジェクション行列の更新

	m_worldProj = XMMatrixPerspectiveFovLH(
		info.fov,			//視野角
		info.aspectRatio,	//アスペクト比
		info.nearZ,			//ニアクリップ距離
		info.farZ			//ファークリップ距離
	);

	//スクリーンカメラ行列の更新
	m_screenView = XMMatrixIdentity();					//カメラの上方

	//スクリーンプロジェクション行列の更新
	m_screenProj = XMMatrixOrthographicLH(
		(int)App::WINDOW_WIDTH,	//画面幅
		(int)App::WINDOW_HEIGHT,	//画面高さ
		0.0f,									//ニアクリップ距離
		1.0f);									//ファークリップ距離
}

//描画
void Renderer::Draw(
	UINT index,									//描画インデックス（未使用）
	ID3D12GraphicsCommandList* p_commandList,	//コマンドリスト
	TextureManager& textureManager				//テクスチャ管理クラス
)
{
	SortDrawList();	//描画リストのソート

	//デスクリプタヒープの設定
	ID3D12DescriptorHeap* heaps[] = { textureManager.GetSrvHeap() };	//SRVヒープの取得
	p_commandList->SetDescriptorHeaps(_countof(heaps), heaps);			//デスクリプタヒープの設定

	//ルートシグネチャの設定
	p_commandList->SetGraphicsRootSignature(m_pRootSignature->GetRootSignature());

	DrawRenderListWorld(p_commandList, textureManager);		//ワールド座標用描画リストの描画
	DrawRenderListEffect(p_commandList, textureManager);	//エフェクト用描画リストの描画
	DrawRenderListScreen(p_commandList, textureManager);	//スクリーン座標用描画リストの描画
}

//フレーム開始
void Renderer::BeginFrame(UINT backIndex)
{
	m_currBackIndex = backIndex;	//現在のバックバッファインデックスを保存
	//描画リストのクリア
	for (auto& drawList : m_drawListWorldNoLight)
	{
		drawList.clear();
	}
	for (auto& drawList : m_drawListWorldLight)
	{
		drawList.clear();
	}
	for (auto& drawList : m_drawListEffect)
	{
		drawList.clear();
	}
	for (auto& drawList : m_drawListScreen)
	{
		drawList.clear();
	}
}

//ワールド座標用描画リストに描画要求を追加
void Renderer::SubmitToWorldList(const WorldRenderModel& info)
{
	for (auto& item : info)
	{
		//ライト有効・無効で分けて追加
		if (item.lightingEnabled) m_drawListWorldLight[item.common.blendMode].push_back(item);	//ライト有効
		else  m_drawListWorldNoLight[item.common.blendMode].push_back(item);					//ライト無効
	}
}

//エフェクト用描画リストに描画要求を追加
void Renderer::SubmitToEffectList(const EffectRenderModel& info)
{
	for (auto& item : info)
	{
		m_drawListEffect[item.common.blendMode].push_back(item);	//描画リストに描画要求を追加
	}
}

//スクリーン座標用描画リストに描画要求を追加
void Renderer::SubmitToScreenList(const WorldRenderModel& info)
{
	for (auto& item : info)
	{
		m_drawListScreen[item.common.blendMode].push_back(item);	//描画リストに描画要求を追加
	}
}

//平行光源情報を設定
void Renderer::SubmitDirectionalLight(const DirectionalLight& light)
{
	m_directionalLight = light;	//平行光源情報を保存
}

//スクリーン座標用描画リストの描画
void Renderer::DrawRenderListWorld(
	ID3D12GraphicsCommandList* p_commandList,
	TextureManager& textureManager
)
{
	int objIndex = 0;

	//ライト無効オブジェクトの描画
	for (size_t i = 0; i < BLEND_MAX; ++i)
	{
		//パイプラインステートの設定
		p_commandList->SetPipelineState(m_pPipelineStateWorldNoLight[i]->GetPipelineState());

		for (size_t j = 0; j < m_drawListWorldNoLight[i].size(); j++)
		{
			// フレームごとのCBVプールを必要数まで確保
			if (objIndex >= m_objectCBWorld[m_currBackIndex].size())
			{
				//新しい定数バッファを作成
				auto* newCb = new ConstantBuffer(m_pDevice, sizeof(PerObjectConstants));

				if (!newCb->GetIsValid())
				{//作成失敗時
					OutputDebugStringA("ConstantBuffer creation failed\n");
					delete newCb;
				}

				//プールに追加
				m_objectCBWorld[m_currBackIndex].push_back(newCb);
			}

			//オブジェクト用定数バッファの取得
			ConstantBuffer* cb = m_objectCBWorld[m_currBackIndex][objIndex];
			auto* ptr = cb->GetPtr<PerObjectConstants>();

			//定数バッファに transform を書く（各オブジェクト専用のメモリ）

			if (m_drawListWorldNoLight[i][j].billboardType != BILLBOARD_NONE)
			{//ビルボードの場合
				//ビルボード用のワールド行列を計算してセット
				ptr->worldMatrix = CalcBillBoard(m_drawListWorldNoLight[i][j]);
			}
			else
			{//通常のワールド行列の場合
				ptr->worldMatrix = m_drawListWorldNoLight[i][j].world;	//ワールド行列
			}
			ptr->worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, ptr->worldMatrix)); //ワールド逆転置行列
			ptr->viewMatrix = m_worldView;					//ビュー行列
			ptr->projMatrix = m_worldProj;					//プロジェクション行列
			ptr->objectColor = m_drawListWorldNoLight[i][j].common.color;	//オブジェクトの色
			ptr->uvRect = m_drawListWorldNoLight[i][j].common.uvRect;		//UV矩形

			//メッシュGPUデータの取得
			auto meshGPU = m_drawListWorldNoLight[i][j].common.pMeshGPU;

			//セットアップ
			auto vbv = meshGPU->GetVertexBuffer()->GetView();						//頂点バッファビューの取得
			auto ibv = meshGPU->GetIndexBuffer()->GetView();						//インデックスバッファビューの取得
			p_commandList->SetGraphicsRootConstantBufferView(0, cb->GetAddress());	//ルートパラメータ0に定数バッファをセット
			p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());			//プリミティブトポロジの設定
			p_commandList->IASetVertexBuffers(0, 1, &vbv);							//頂点バッファの設定
			p_commandList->IASetIndexBuffer(&ibv);									//インデックスバッファの設定

			//SRVの設定
			auto heapHandle = textureManager.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();						//SRVヒープのGPUハンドルを取得
			uint32_t idx = m_drawListWorldNoLight[i][j].common.srvIndex;
			if (idx == UINT32_MAX)
			{
				idx = textureManager.GetDefaultWhiteTextureIndex(); //白テクスチャのインデックスを使用
			}
			auto gpuHandle = heapHandle;
			gpuHandle.ptr += static_cast<UINT64>(idx) * textureManager.GetSrvIncrementSize();
			p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

			//描画コマンドの発行
			p_commandList->DrawIndexedInstanced(	//描画コマンド
				meshGPU->GetIndexCount(),		//インデックス数
				1,								//インスタンス数
				m_drawListWorldNoLight[i][j].startIndex,	//スタートインデックス位置
				m_drawListWorldNoLight[i][j].baseVertex,	//ベース頂点位置
				0								//スタートインスタンス位置
			);

			objIndex++;	//オブジェクト用定数バッファのインデックスを進める
		}
	}

	//ライト有効オブジェクトの描画
	for (size_t i = 0; i < BLEND_MAX; ++i)
	{
		//パイプラインステートの設定
		p_commandList->SetPipelineState(m_pPipelineStateWorldLight[i]->GetPipelineState());

		for (size_t j = 0; j < m_drawListWorldLight[i].size(); j++)
		{
			// フレームごとのCBVプールを必要数まで確保
			if (objIndex >= m_objectCBWorld[m_currBackIndex].size())
			{
				//新しい定数バッファを作成
				auto* newCb = new ConstantBuffer(m_pDevice, sizeof(PerObjectConstants));

				if (!newCb->GetIsValid())
				{//作成失敗時
					OutputDebugStringA("ConstantBuffer creation failed\n");
					delete newCb;
				}

				//プールに追加
				m_objectCBWorld[m_currBackIndex].push_back(newCb);
			}

			//オブジェクト用定数バッファの取得
			ConstantBuffer* cb = m_objectCBWorld[m_currBackIndex][objIndex];
			auto* ptr = cb->GetPtr<PerObjectConstants>();

			//定数バッファに transform を書く（各オブジェクト専用のメモリ）

			if (m_drawListWorldLight[i][j].billboardType != BILLBOARD_NONE)
			{//ビルボードの場合
				//ビルボード用のワールド行列を計算してセット
				ptr->worldMatrix = CalcBillBoard(m_drawListWorldLight[i][j]);
			}
			else
			{//通常のワールド行列の場合
				ptr->worldMatrix = m_drawListWorldLight[i][j].world;	//ワールド行列
			}
			ptr->worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, ptr->worldMatrix)); //ワールド逆転置行列
			ptr->viewMatrix = m_worldView;					//ビュー行列
			ptr->projMatrix = m_worldProj;					//プロジェクション行列
			ptr->objectColor = m_drawListWorldLight[i][j].common.color;	//オブジェクトの色
			ptr->uvRect = m_drawListWorldLight[i][j].common.uvRect;		//UV矩形
			XMFLOAT4 direction_intensity =
			{
				m_directionalLight.direction.x,
				m_directionalLight.direction.y,
				m_directionalLight.direction.z,
				m_directionalLight.intensity
			};
			XMFLOAT4 color_amobient = XMFLOAT4(
				m_directionalLight.color.x,
				m_directionalLight.color.y,
				m_directionalLight.color.z,
				m_directionalLight.ambient
			);
			ptr->lightDir_Intensity = direction_intensity;
			ptr->lightColor_Ambient = color_amobient;

			//メッシュGPUデータの取得
			auto meshGPU = m_drawListWorldLight[i][j].common.pMeshGPU;

			//セットアップ
			auto vbv = meshGPU->GetVertexBuffer()->GetView();						//頂点バッファビューの取得
			auto ibv = meshGPU->GetIndexBuffer()->GetView();						//インデックスバッファビューの取得
			p_commandList->SetGraphicsRootConstantBufferView(0, cb->GetAddress());	//ルートパラメータ0に定数バッファをセット
			p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());			//プリミティブトポロジの設定
			p_commandList->IASetVertexBuffers(0, 1, &vbv);							//頂点バッファの設定
			p_commandList->IASetIndexBuffer(&ibv);									//インデックスバッファの設定

			//SRVの設定
			auto heapHandle = textureManager.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();	//SRVヒープのGPUハンドルを取得
			uint32_t idx = m_drawListWorldLight[i][j].common.srvIndex;
			if (idx == UINT32_MAX)
			{
				idx = textureManager.GetDefaultWhiteTextureIndex(); //白テクスチャのインデックスを使用
			}
			auto gpuHandle = heapHandle;
			gpuHandle.ptr += static_cast<UINT64>(idx) * textureManager.GetSrvIncrementSize();
			p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

			//描画コマンドの発行
			p_commandList->DrawIndexedInstanced(	//描画コマンド
				meshGPU->GetIndexCount(),		//インデックス数
				1,								//インスタンス数
				m_drawListWorldLight[i][j].startIndex,	//スタートインデックス位置
				m_drawListWorldLight[i][j].baseVertex,	//ベース頂点位置
				0								//スタートインスタンス位置
			);

			objIndex++;	//オブジェクト用定数バッファのインデックスを進める
		}
	}
}

//エフェクト用描画リストの描画
void Renderer::DrawRenderListEffect(ID3D12GraphicsCommandList* p_commandList, TextureManager& textureManager)
{
	int objIndex = 0; //オブジェクト用定数バッファのインデックス

	for (int i = 0; i < BLEND_MAX; i++)
	{
		//パイプラインステートの設定
		p_commandList->SetPipelineState(m_pPipelineStateEffect[i]->GetPipelineState());

		for (size_t j = 0; j < m_drawListEffect[i].size(); j++)
		{
			// フレームごとのCBVプールを必要数まで確保
			if (objIndex >= m_objectCBEffect[m_currBackIndex].size())
			{
				//新しい定数バッファを作成
				auto* newCb = new ConstantBuffer(m_pDevice, sizeof(EffectCB));

				if (!newCb->GetIsValid())
				{//作成失敗時
					OutputDebugStringA("ConstantBuffer creation failed\n");
					delete newCb;
				}

				//プールに追加
				m_objectCBEffect[m_currBackIndex].push_back(newCb);
			}

			//オブジェクト用定数バッファの取得
			ConstantBuffer* cb = m_objectCBEffect[m_currBackIndex][objIndex];	//エフェクト用定数バッファ
			auto* ptr = cb->GetPtr<EffectCB>();									//定数バッファのポインタ取得
			ptr->viewProj = XMMatrixTranspose(m_worldView * m_worldProj);		//ビュー射影行列
			ptr->camUp = m_cameraInfo->up;										//カメラの上方向ベクトル
			ptr->camRight = m_cameraInfo->right;								//カメラの右方向ベクトル
			ptr->center = m_drawListEffect[i][j].center;						//ワールド行列
			ptr->size = m_drawListEffect[i][j].size;							//ビュー行列
			ptr->color = m_drawListEffect[i][j].common.color;					//オブジェクトの色
			ptr->uvRect = m_drawListEffect[i][j].common.uvRect;					//UV矩形
			p_commandList->SetGraphicsRootConstantBufferView(2, cb->GetAddress());	//ルートパラメータ2に定数バッファをセット

			//エフェクト描画要求の取得
			auto& effectDrawInfo = m_drawListEffect[i][j];

			//メッシュGPUデータの取得
			auto meshGPU = effectDrawInfo.common.pMeshGPU;

			//セットアップ
			auto vbv = meshGPU->GetVertexBuffer()->GetView();				//頂点バッファビューの取得
			auto ibv = meshGPU->GetIndexBuffer()->GetView();				//インデックスバッファビューの取得
			p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());	//プリミティブトポロジの設定
			p_commandList->IASetVertexBuffers(0, 1, &vbv);					//頂点バッファの設定
			p_commandList->IASetIndexBuffer(&ibv);							//インデックスバッファの設定

			//SRVの設定
			auto heapHandle = textureManager.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();						//SRVヒープのGPUハンドルを取得
			uint32_t idx = m_drawListEffect[i][j].common.srvIndex;
			if (idx == UINT32_MAX)
			{
				idx = textureManager.GetDefaultWhiteTextureIndex(); //白テクスチャのインデックスを使用
			}
			auto gpuHandle = heapHandle;
			gpuHandle.ptr += static_cast<UINT64>(idx) * textureManager.GetSrvIncrementSize();
			p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

			//描画コマンドの発行
			p_commandList->DrawIndexedInstanced(	//描画コマンド
				meshGPU->GetIndexCount(),	//インデックス数
				1,							//インスタンス数
				0,							//スタートインデックス位置
				0,							//ベース頂点位置
				0							//スタートインスタンス位置
			);

			objIndex++;		//オブジェクト用定数バッファのインデックスを進める
		}
	}
}

//スクリーン座標用描画リストの描画
void Renderer::DrawRenderListScreen(
	ID3D12GraphicsCommandList* p_commandList,
	TextureManager& textureManager
)
{
	int objIndex = 0;

	// ドロー要求を順に処理
	for (size_t i = 0; i < BLEND_MAX; ++i)
	{
		//パイプラインステートの設定
		p_commandList->SetPipelineState(m_pPipelineStateScreen[i]->GetPipelineState());

		for (size_t j = 0; j < m_drawListScreen[i].size(); j++)
		{
			// フレームごとのCBVプールを必要数まで確保
			if (objIndex >= m_objectCBScreen[m_currBackIndex].size())
			{
				//新しい定数バッファを作成
				auto* newCb = new ConstantBuffer(m_pDevice, sizeof(PerObjectConstants));

				if (!newCb->GetIsValid())
				{//作成失敗時
					OutputDebugStringA("ConstantBuffer creation failed\n");
					delete newCb;
				}

				//プールに追加
				m_objectCBScreen[m_currBackIndex].push_back(newCb);
			}

			//オブジェクト用定数バッファの取得
			ConstantBuffer* cb = m_objectCBScreen[m_currBackIndex][objIndex];
			auto* ptr = cb->GetPtr<PerObjectConstants>();

			//定数バッファに transform を書く（各オブジェクト専用のメモリ）
			ptr->worldMatrix = m_drawListScreen[i][j].world;	//ワールド行列
			ptr->worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, ptr->worldMatrix)); //ワールド逆転置行列
			ptr->viewMatrix = m_screenView;						//ビュー行列
			ptr->projMatrix = m_screenProj;						//プロジェクション行列
			ptr->objectColor = m_drawListScreen[i][j].common.color;	//オブジェクトの色
			ptr->uvRect = m_drawListScreen[i][j].common.uvRect;		//UV矩形

			//メッシュGPUデータの取得
			auto meshGPU = m_drawListScreen[i][j].common.pMeshGPU;

			//セットアップ
			auto vbv = meshGPU->GetVertexBuffer()->GetView();						//頂点バッファビューの取得
			auto ibv = meshGPU->GetIndexBuffer()->GetView();						//インデックスバッファビューの取得
			p_commandList->SetGraphicsRootConstantBufferView(0, cb->GetAddress());	//ルートパラメータ0に定数バッファをセット
			p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());			//プリミティブトポロジの設定
			p_commandList->IASetVertexBuffers(0, 1, &vbv);							//頂点バッファの設定
			p_commandList->IASetIndexBuffer(&ibv);									//インデックスバッファの設定

			//SRVの設定
			auto heapHandle = textureManager.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();						//SRVヒープのGPUハンドルを取得
			uint32_t idx = m_drawListScreen[i][j].common.srvIndex;
			if (idx == UINT32_MAX)
			{
				idx = textureManager.GetDefaultWhiteTextureIndex(); //白テクスチャのインデックスを使用
			}
			auto gpuHandle = heapHandle;
			gpuHandle.ptr += static_cast<UINT64>(idx) * textureManager.GetSrvIncrementSize();
			p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

			//描画コマンドの発行
			p_commandList->DrawIndexedInstanced(	//描画コマンド
				meshGPU->GetIndexCount(),		//インデックス数
				1,								//インスタンス数
				m_drawListScreen[i][j].startIndex,	//スタートインデックス位置
				m_drawListScreen[i][j].baseVertex,	//ベース頂点位置
				0								//スタートインスタンス位置
			);

			objIndex++;	//オブジェクト用定数バッファのインデックスを進める
		}
	}
}

//描画リストのソート
void Renderer::SortDrawList()
{
	SortDrawListOpaque();			//不透明オブジェクトの描画リストソート
	SortDrawListTransparent();		//透明オブジェクトの描画リストソート
}

//不透明オブジェクトの描画リストソート
void Renderer::SortDrawListOpaque()
{
	auto cameraPos = m_cameraInfo->position;	//カメラ位置

	//距離の二乗を計算するラムダ式
	auto dist2 = [](const XMFLOAT3& a, const XMFLOAT3& b)
		{
			float dx = a.x - b.x;
			float dy = a.y - b.y;
			float dz = a.z - b.z;
			return dx * dx + dy * dy + dz * dz;
		};
	//カメラから近い順にソート
	//ワールド座標用描画リスト
	//OPAQUE
	std::sort(
		m_drawListWorldLight[BLEND_OPAQUE].begin(),	//ソート開始位置
		m_drawListWorldLight[BLEND_OPAQUE].end(),	//ソート終了位置
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return dist2(a.position, cameraPos) < dist2(b.position, cameraPos);
		}
	);
	std::sort(
		m_drawListWorldNoLight[BLEND_OPAQUE].begin(),	//ソート開始位置
		m_drawListWorldNoLight[BLEND_OPAQUE].end(),	//ソート終了位置
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return dist2(a.position, cameraPos) < dist2(b.position, cameraPos);
		}
	);
	//MASKED
	std::sort(
		m_drawListWorldLight[BLEND_MASKED].begin(),	//ソート開始位置
		m_drawListWorldLight[BLEND_MASKED].end(),	//ソート終了位置
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return dist2(a.position, cameraPos) < dist2(b.position, cameraPos);
		}
	);
	std::sort(
		m_drawListWorldNoLight[BLEND_MASKED].begin(),	//ソート開始位置
		m_drawListWorldNoLight[BLEND_MASKED].end(),	//ソート終了位置
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return dist2(a.position, cameraPos) < dist2(b.position, cameraPos);
		}
	);

	//エフェクト用描画リスト
	//OPAQUE
	std::sort(
		m_drawListEffect[BLEND_OPAQUE].begin(),	//ソート開始位置
		m_drawListEffect[BLEND_OPAQUE].end(),	//ソート終了位置
		[&](const EffectRenderInfo& a, const EffectRenderInfo& b)
		{
			return dist2(a.center, cameraPos) < dist2(b.center, cameraPos);
		}
	);
	//MASKED
	std::sort(
		m_drawListEffect[BLEND_MASKED].begin(),	//ソート開始位置
		m_drawListEffect[BLEND_MASKED].end(),	//ソート終了位置
		[&](const EffectRenderInfo& a, const EffectRenderInfo& b)
		{
			return dist2(a.center, cameraPos) < dist2(b.center, cameraPos);
		}
	);

	//スクリーン座標用描画リスト
	//OPAQUE
	std::sort(
		m_drawListScreen[BLEND_OPAQUE].begin(),	//ソート開始位置
		m_drawListScreen[BLEND_OPAQUE].end(),	//ソート終了位置
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return dist2(a.position, cameraPos) < dist2(b.position, cameraPos);
		}
	);
	//MASKED
	std::sort(
		m_drawListScreen[BLEND_MASKED].begin(),	//ソート開始位置
		m_drawListScreen[BLEND_MASKED].end(),	//ソート終了位置
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return dist2(a.position, cameraPos) < dist2(b.position, cameraPos);
		}
	);
}

//透明オブジェクトの描画リストソート
void Renderer::SortDrawListTransparent()
{
	//カメラ位置と正面ベクトルの計算
	XMFLOAT3 cameraPos = m_cameraInfo->position;	//カメラ位置
	XMFLOAT3 cameraForward = {						//カメラ正面ベクトル
		m_cameraInfo->target.x - m_cameraInfo->position.x,
		m_cameraInfo->target.y - m_cameraInfo->position.y,
		m_cameraInfo->target.z - m_cameraInfo->position.z
	};

	//正規化
	XMVECTOR vF = XMVector3Normalize(XMLoadFloat3(&cameraForward));
	XMStoreFloat3(&cameraForward, vF);

	auto depthFar = [&](const XMFLOAT3& r, const MeshGPU* pMeshGPU)
		{
			//カメラからの奥行きを計算
			float vx = r.x - cameraPos.x;
			float vy = r.y - cameraPos.y;
			float vz = r.z - cameraPos.z;
			float centerDepth =
				vx * cameraForward.x + vy * cameraForward.y + vz * cameraForward.z;

			//ソート用の半径を足す
			float radius = pMeshGPU->GetSortRadius();
			return centerDepth + radius;
		};

	//カメラから遠い順にソート
	//ワールド座標用描画リスト
	std::sort(
		m_drawListWorldLight[BLEND_TRANSPARENT].begin(),
		m_drawListWorldLight[BLEND_TRANSPARENT].end(),
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return depthFar(a.position, a.common.pMeshGPU) > depthFar(b.position, b.common.pMeshGPU);
		}
	);
	std::sort(
		m_drawListWorldNoLight[BLEND_TRANSPARENT].begin(),
		m_drawListWorldNoLight[BLEND_TRANSPARENT].end(),
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return depthFar(a.position, a.common.pMeshGPU) > depthFar(b.position, b.common.pMeshGPU);
		}
	);

	//エフェクト用描画リスト
	std::sort(
		m_drawListEffect[BLEND_TRANSPARENT].begin(),
		m_drawListEffect[BLEND_TRANSPARENT].end(),
		[&](const EffectRenderInfo& a, const EffectRenderInfo& b)
		{
			return depthFar(a.center, a.common.pMeshGPU) > depthFar(b.center, b.common.pMeshGPU);
		}
	);

	//スクリーン座標用描画リスト
	std::sort(
		m_drawListScreen[BLEND_TRANSPARENT].begin(),
		m_drawListScreen[BLEND_TRANSPARENT].end(),
		[&](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return depthFar(a.position, a.common.pMeshGPU) > depthFar(b.position, b.common.pMeshGPU);
		}
	);
}

//ビルボード計算
XMMATRIX Renderer::CalcBillBoard(const WorldRenderInfo& info)
{
	XMVECTOR objPos = XMLoadFloat3(&info.position);

	XMVECTOR cameraPos = XMLoadFloat3(&m_cameraInfo->position);
	XMVECTOR upWorld = XMVectorSet(0, 1, 0, 0);

	XMVECTOR toCam = XMVectorSubtract(cameraPos, objPos);

	if (info.billboardType == BILLBOARD_TYPE::BILLBOARD_FIX_X)
	{
		toCam = XMVectorSet(
			0.0f,
			XMVectorGetY(toCam),
			XMVectorGetZ(toCam),
			0.0f
		);
	}
	else if (info.billboardType == BILLBOARD_TYPE::BILLBOARD_FIX_Y)
	{
		toCam = XMVectorSet(
			XMVectorGetX(toCam),
			0.0f,
			XMVectorGetZ(toCam),
			0.0f
		);
	}
	else if (info.billboardType == BILLBOARD_TYPE::BILLBOARD_FIX_Z)
	{
		toCam = XMVectorSet(
			XMVectorGetX(toCam),
			XMVectorGetY(toCam),
			0.0f,
			0.0f
		);
	}

	XMVECTOR forward = XMVector3Normalize(toCam);
	forward = XMVectorNegate(forward);
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(upWorld, forward));
	XMVECTOR up = XMVector3Cross(forward, right);

	XMFLOAT3 r, u, f;
	XMStoreFloat3(&r, right);
	XMStoreFloat3(&u, up);
	XMStoreFloat3(&f, forward);

	XMMATRIX rot =
		XMMATRIX(
			r.x, r.y, r.z, 0.0f,
			u.x, u.y, u.z, 0.0f,
			f.x, f.y, f.z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);

	XMMATRIX scale = XMMatrixScaling(info.scale.x, info.scale.y, info.scale.z);

	XMMATRIX trans = XMMatrixTranslation(info.position.x, info.position.y, info.position.z);

	return scale * rot * trans;
}
