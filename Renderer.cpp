#include "Renderer.h"
#include <algorithm>
#include <vector>
#include "App.h"
#include "Engine.h"
#include "TextureManager.h"
#include "d3dx12.h"
#include "SharedStruct.h"
#include "AssimpLoader.h"
#include "ShaderLibrary.h"

using namespace DirectX;

//デストラクタ
Renderer::~Renderer()
{
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

		for (auto& pCB : m_objectCBScreen[i])
		{
			if (pCB)
			{
				delete pCB;
				pCB = nullptr;
			}
		}
		m_objectCBScreen[i].clear();
	}

	//パイプラインステートオブジェクトの解放
	for (auto& pair : m_psoMap)
	{
		if (pair.second)
		{
			delete pair.second;
			pair.second = nullptr;
		}
	}
	m_psoMap.clear();

	//ルートシグネチャの解放
	if (m_pRootSignature)
	{
		delete m_pRootSignature;
		m_pRootSignature = nullptr;
	}
}

//初期化
void Renderer::Initialize(ID3D12Device* pDevice, CameraInfo* pInfo, TextureManager* textureManager)
{
	m_pDevice = pDevice;					//デバイスの保存
	m_cameraInfo = pInfo;					//カメラ情報構造体の保存
	m_pTextureManager = textureManager;		//テクスチャ管理クラスの保存

	m_directionalLight = {}; //初期化

	//ルートシグネチャの生成
	m_pRootSignature = new RootSignature(m_pDevice);

	//シェーダーライブラリの生成
	m_pShaderLibrary = new ShaderLibrary();

	//ベーシックPSOの生成
	PSOKey defaultKey{};
	m_pDefaultPSO = CreatePipelineStateObject(defaultKey);
}

//更新
void Renderer::Update(UINT currentBackBufferIndex, CameraInfo& info)
{
	m_cameraInfo = &info;	//カメラ情報構造体の保存

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
	ID3D12GraphicsCommandList* p_commandList	//コマンドリスト
)
{
	//デスクリプタヒープの設定
	ID3D12DescriptorHeap* heaps[] = { m_pTextureManager->GetSrvHeap() };	//SRVヒープの取得
	p_commandList->SetDescriptorHeaps(_countof(heaps), heaps);			//デスクリプタヒープの設定

	//ルートシグネチャの設定
	p_commandList->SetGraphicsRootSignature(m_pRootSignature->GetRootSignature());

	//パイプラインステートオブジェクトごとにソート
	SortRenderListWorldByPSO();
	SortRenderListScreenByPSO();

	DrawTempRenderListWorld(p_commandList);
	DrawTempRenderListScreen(p_commandList);
}

//フレーム開始
void Renderer::BeginFrame(UINT backIndex)
{
	m_currBackIndex = backIndex;	//現在のバックバッファインデックスを保存
	m_tempWorldRenderList.clear();	//一時描画リストのクリア
	m_tempScreenRenderList.clear();	//一時描画リストのクリア
}

//ワールド座標用描画リストに描画要求を追加
void Renderer::SubmitToWorldList(const WorldRenderModel& info)
{
	for (const auto& item : info)
	{
		WorldRenderInfo itemRef = item;
		NormalizeKeyForRenderQueueWorld(itemRef);						//レンダリングキューに応じたパイプラインステートオブジェクトキーの正規化
		itemRef.common.sortDepth = CalcSortDepth(item.position);	//ソート用深度の計算と設定
		if (itemRef.common.srvIndex == UINT32_MAX)
		{
			itemRef.common.srvIndex = m_pTextureManager->GetDefaultWhiteTextureIndex(); //白テクスチャのインデックスを使用
		}
		m_tempWorldRenderList.push_back(itemRef);	//一時描画リストに追加
	}
}

//スクリーン座標用描画リストに描画要求を追加
void Renderer::SubmitToScreenList(const WorldRenderModel& info)
{
	for (const auto& item : info)
	{
		WorldRenderInfo itemRef = item;
		NormalizeKeyForRenderQueueScreen(itemRef);
		itemRef.common.sortDepth = CalcSortDepth(item.position); //ソート用深度の計算と設定
		if (itemRef.common.srvIndex == UINT32_MAX)
		{
			itemRef.common.srvIndex = m_pTextureManager->GetDefaultWhiteTextureIndex(); //白テクスチャのインデックスを使用
		}
		m_tempScreenRenderList.push_back(itemRef);	//一時描画リストに追加
	}
}

//平行光源情報を設定
void Renderer::SubmitDirectionalLight(const DirectionalLight& light)
{
	m_directionalLight = light;	//平行光源情報を保存
}

//一時描画リストの描画
void Renderer::DrawTempRenderListWorld(ID3D12GraphicsCommandList* p_commandList)
{
	PSOKey compare{};

	for (size_t i = 0; i < m_tempWorldRenderList.size(); i++)
	{
		//パイプラインステートオブジェクトの設定
		if (i == 0)
		{
			auto pso = GetPipelineStateObject(m_tempWorldRenderList[i].common.psoKey);	//パイプラインステートを取得
			p_commandList->SetPipelineState(pso->GetPipelineState());					//パイプラインステートをセット
			compare = m_tempWorldRenderList[i].common.psoKey;
		}
		else if (compare != m_tempWorldRenderList[i].common.psoKey)
		{
			auto pso = GetPipelineStateObject(m_tempWorldRenderList[i].common.psoKey);	//パイプラインステートを取得
			p_commandList->SetPipelineState(pso->GetPipelineState());					//パイプラインステートをセット
		}

		compare = m_tempWorldRenderList[i].common.psoKey;

		// フレームごとのCBVプールを必要数まで確保
		if (i >= m_objectCBWorld[m_currBackIndex].size())
		{
			//新しい定数バッファを作成
			auto* newCb = new ConstantBuffer(m_pDevice, sizeof(PerObjectConstants));

			if (!newCb->GetIsValid())
			{//作成失敗時
				OutputDebugStringA("ConstantBuffer creation failed\n");
				delete newCb;
				break;
			}

			//プールに追加
			m_objectCBWorld[m_currBackIndex].push_back(newCb);
		}

		//オブジェクト用定数バッファの取得
		ConstantBuffer* cb = m_objectCBWorld[m_currBackIndex][i];
		auto* ptr = cb->GetPtr<PerObjectConstants>();

		//定数バッファに transform を書く（各オブジェクト専用のメモリ）

		if (m_tempWorldRenderList[i].billboardType != BILLBOARD_NONE)
		{//ビルボードの場合
			//ビルボード用のワールド行列を計算してセット
			ptr->worldMatrix = CalcBillBoard(m_tempWorldRenderList[i]);
		}
		else
		{//通常のワールド行列の場合
			ptr->worldMatrix = m_tempWorldRenderList[i].world;	//ワールド行列
		}
		ptr->worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, ptr->worldMatrix)); //ワールド逆転置行列
		ptr->viewMatrix = m_worldView;					//ビュー行列
		ptr->projMatrix = m_worldProj;					//プロジェクション行列
		ptr->objectColor = m_tempWorldRenderList[i].common.color;	//オブジェクトの色
		ptr->uvRect = m_tempWorldRenderList[i].common.uvRect;		//UV矩形
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
		auto meshGPU = m_tempWorldRenderList[i].common.pMeshGPU;

		//セットアップ
		auto vbv = meshGPU->GetVertexBuffer()->GetView();						//頂点バッファビューの取得
		auto ibv = meshGPU->GetIndexBuffer()->GetView();						//インデックスバッファビューの取得
		p_commandList->SetGraphicsRootConstantBufferView(0, cb->GetAddress());	//ルートパラメータ0に定数バッファをセット
		p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());			//プリミティブトポロジの設定
		p_commandList->IASetVertexBuffers(0, 1, &vbv);							//頂点バッファの設定
		p_commandList->IASetIndexBuffer(&ibv);									//インデックスバッファの設定

		//SRVの設定
		auto heapHandle = m_pTextureManager->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();						//SRVヒープのGPUハンドルを取得
		uint32_t idx = m_tempWorldRenderList[i].common.srvIndex;
		auto gpuHandle = heapHandle;
		gpuHandle.ptr += static_cast<UINT64>(idx) * m_pTextureManager->GetSrvIncrementSize();
		p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

		//描画コマンドの発行
		p_commandList->DrawIndexedInstanced(	//描画コマンド
			meshGPU->GetIndexCount(),				//インデックス数
			1,										//インスタンス数
			m_tempWorldRenderList[i].startIndex,	//スタートインデックス位置
			m_tempWorldRenderList[i].baseVertex,	//ベース頂点位置
			0										//スタートインスタンス位置
		);
	}
}

void Renderer::DrawTempRenderListScreen(ID3D12GraphicsCommandList* p_commandList)
{
	PSOKey compare{};

	for (size_t i = 0; i < m_tempScreenRenderList.size(); i++)
	{
		//パイプラインステートオブジェクトの設定
		if (i == 0)
		{
			auto pso = GetPipelineStateObject(m_tempScreenRenderList[i].common.psoKey);	//パイプラインステートを取得
			p_commandList->SetPipelineState(pso->GetPipelineState());					//パイプラインステートをセット
			compare = m_tempScreenRenderList[i].common.psoKey;
		}
		else if (compare != m_tempScreenRenderList[i].common.psoKey)
		{
			auto pso = GetPipelineStateObject(m_tempScreenRenderList[i].common.psoKey);	//パイプラインステートを取得
			p_commandList->SetPipelineState(pso->GetPipelineState());					//パイプラインステートをセット
		}

		compare = m_tempScreenRenderList[i].common.psoKey;

		// フレームごとのCBVプールを必要数まで確保
		if (i >= m_objectCBScreen[m_currBackIndex].size())
		{
			//新しい定数バッファを作成
			auto* newCb = new ConstantBuffer(m_pDevice, sizeof(PerObjectConstants));

			if (!newCb->GetIsValid())
			{//作成失敗時
				OutputDebugStringA("ConstantBuffer creation failed\n");
				delete newCb;
				break;
			}

			//プールに追加
			m_objectCBScreen[m_currBackIndex].push_back(newCb);
		}

		//オブジェクト用定数バッファの取得
		ConstantBuffer* cb = m_objectCBScreen[m_currBackIndex][i];
		auto* ptr = cb->GetPtr<PerObjectConstants>();

		//定数バッファに transform を書く（各オブジェクト専用のメモリ）

		if (m_tempScreenRenderList[i].billboardType != BILLBOARD_NONE)
		{//ビルボードの場合
			//ビルボード用のワールド行列を計算してセット
			ptr->worldMatrix = CalcBillBoard(m_tempScreenRenderList[i]);
		}
		else
		{//通常のワールド行列の場合
			ptr->worldMatrix = m_tempScreenRenderList[i].world;	//ワールド行列
		}
		ptr->worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, ptr->worldMatrix)); //ワールド逆転置行列
		ptr->viewMatrix = m_screenView;					//ビュー行列
		ptr->projMatrix = m_screenProj;					//プロジェクション行列
		ptr->objectColor = m_tempScreenRenderList[i].common.color;	//オブジェクトの色
		ptr->uvRect = m_tempScreenRenderList[i].common.uvRect;		//UV矩形
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
		auto meshGPU = m_tempScreenRenderList[i].common.pMeshGPU;

		//セットアップ
		auto vbv = meshGPU->GetVertexBuffer()->GetView();						//頂点バッファビューの取得
		auto ibv = meshGPU->GetIndexBuffer()->GetView();						//インデックスバッファビューの取得
		p_commandList->SetGraphicsRootConstantBufferView(0, cb->GetAddress());	//ルートパラメータ0に定数バッファをセット
		p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());			//プリミティブトポロジの設定
		p_commandList->IASetVertexBuffers(0, 1, &vbv);							//頂点バッファの設定
		p_commandList->IASetIndexBuffer(&ibv);									//インデックスバッファの設定

		//SRVの設定
		auto heapHandle = m_pTextureManager->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();						//SRVヒープのGPUハンドルを取得
		uint32_t idx = m_tempScreenRenderList[i].common.srvIndex;
		auto gpuHandle = heapHandle;
		gpuHandle.ptr += static_cast<UINT64>(idx) * m_pTextureManager->GetSrvIncrementSize();
		p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

		//描画コマンドの発行
		p_commandList->DrawIndexedInstanced(	//描画コマンド
			meshGPU->GetIndexCount(),				//インデックス数
			1,										//インスタンス数
			m_tempScreenRenderList[i].startIndex,	//スタートインデックス位置
			m_tempScreenRenderList[i].baseVertex,	//ベース頂点位置
			0										//スタートインスタンス位置
		);
	}
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

//ソート深度計算
float Renderer::CalcSortDepth(const DirectX::XMFLOAT3& position)
{
	return Dot(
		XMFLOAT3(
			position.x - m_cameraInfo->position.x,
			position.y - m_cameraInfo->position.y,
			position.z - m_cameraInfo->position.z
		),
		m_cameraInfo->forward
	);
}

//PSO取得
PipelineState* Renderer::GetPipelineStateObject(PSOKey key)
{
	auto it = m_psoMap.find(key);	//PSOマップから検索
	if (it != m_psoMap.end())
	{//見つかった場合はそれを返す
		return it->second;
	}
	else
	{//見つからなかった場合は新規作成
		PipelineState* pso = CreatePipelineStateObject(key);

		if (!pso)
		{//作成失敗時はデフォルトPSOを返す
			return m_pDefaultPSO;
		}

		return pso;
	}
}

//PSO生成
PipelineState* Renderer::CreatePipelineStateObject(const PSOKey& key)
{
	PipelineState* pso = nullptr;

	// Get shaders
	auto vs = m_pShaderLibrary->GetVS(key.vsKey.fileID, key.vsKey.entryID, key.vsKey.defines, key.commonDefines);
	auto ps = m_pShaderLibrary->GetPS(key.psKey.fileID, key.psKey.entryID, key.psKey.defines, key.commonDefines);
	if (!vs || !ps)
	{
		OutputDebugStringA("Shader blob missing\n");
		delete pso;
		return nullptr;
	}

	// Create a new pipeline state object
	pso = new PipelineState(m_pDevice);
	pso->SetInputLayout(Vertex::InputLayout);
	pso->SetRootSignature(m_pRootSignature->GetRootSignature());
	pso->SetVertexShader(vs.Get());
	pso->SetPixelShader(ps.Get());
	pso->SetBlendMode(key.blend);
	pso->SetDepthMode(key.depth);
	pso->SetCullMode(key.cull);
	pso->Create();

	// Check if creation was successful
	if (!pso->IsValid())
	{
		OutputDebugStringA("PipelineState creation failed\n");
		delete pso;
		return nullptr;
	}

	m_psoMap.emplace(key, pso);
	return pso;
}

//描画リストをPSO別にソート
void Renderer::SortRenderListWorldByPSO()
{
	//一時描画リストをレンダリングキュー別に分割
	std::vector<WorldRenderInfo> opaque;
	std::vector<WorldRenderInfo> transparent;

	//分割処理
	for (const auto& renderInfo : m_tempWorldRenderList)
	{
		switch (renderInfo.common.renderQueue)
		{
		case RENDER_QUEUE_OPAQUE:
			opaque.push_back(renderInfo);
			break;
		case RENDER_QUEUE_TRANSPARENT:
			transparent.push_back(renderInfo);
			break;
		default:
			break;
		}
	}

	//不透明オブジェクトをPSOキーでソート
	std::stable_sort(opaque.begin(), opaque.end(),
		[](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return OpaqueLess(a, b);
		});

	//透明オブジェクトを深度とPSOキーでソート
	std::stable_sort(transparent.begin(), transparent.end(),
		[](const WorldRenderInfo& a, const WorldRenderInfo& b)
		{
			return TransparentLess(a, b);
		});


	//ソート済みリストを結合
	m_tempWorldRenderList.clear();
	m_tempWorldRenderList.insert(m_tempWorldRenderList.end(), opaque.begin(), opaque.end());
	m_tempWorldRenderList.insert(m_tempWorldRenderList.end(), transparent.begin(), transparent.end());
}

void Renderer::SortRenderListScreenByPSO()
{
}

//レンダリングキュー取得
void Renderer::NormalizeKeyForRenderQueueWorld(WorldRenderInfo& info)
{
	//レンダリングキューの設定
	info.common.renderQueue = GetRenderQueueFromBlendMode(info.common.psoKey.blend);
	//透明 or 加算レンダリングキューの場合、深度書き込みを無効化
	if (info.common.renderQueue == RENDER_QUEUE_TRANSPARENT
		&& info.common.psoKey.depth != DEPTH_MODE::DEPTH_DISABLE)
	{
		info.common.psoKey.depth = DEPTH_MODE::DEPTH_TEST_NO_WRITE;
	}
}

void Renderer::NormalizeKeyForRenderQueueScreen(WorldRenderInfo& info)
{
	if (info.common.psoKey.depth != DEPTH_MODE::DEPTH_DISABLE)
	{
		info.common.psoKey.depth = DEPTH_MODE::DEPTH_DISABLE;
	}
}