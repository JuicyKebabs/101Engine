#include "Renderer.h"
#include <algorithm>
#include <vector>
#include "App/App.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Graphics/ShaderLibrary.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Component/Camera.h"

using namespace DirectX;

//デストラクタ
Renderer::~Renderer()
{
	m_psoMap.clear();
}

//初期化
void Renderer::Initialize(ID3D12Device* pDevice, DescriptorHeapAllocator* pDescriptorHeapAllocator, TextureManager* pTextureManager)
{
	m_pDevice = pDevice;
	m_pDescriptorHeapAllocator = pDescriptorHeapAllocator;
	m_pTextureManager = pTextureManager;

	m_directionalLight = {}; //初期化

	//ルートシグネチャの生成
	m_pRootSignature = std::make_unique<RootSignature>(m_pDevice);

	//シェーダーライブラリの生成
	m_pShaderLibrary = std::make_unique<ShaderLibrary>();

	//ベーシックPSOの生成
	PSOKey defaultKey{};
	m_pDefaultPSO = CreatePipelineStateObject(defaultKey);

	//ポストプロセス用PSOキーの設定
	PreparePostProcessKey();
}

//更新
void Renderer::Update(UINT currentBackBufferIndex, const CameraInfo& info)
{
}

//フレーム開始
void Renderer::BeginFrame(ID3D12GraphicsCommandList* p_commandList)
{
	// Set descriptor heaps
	ID3D12DescriptorHeap* heaps[] = { m_pDescriptorHeapAllocator->GetCbvSrvUavHeap().GetHeap() };
	p_commandList->SetDescriptorHeaps(_countof(heaps), heaps);				// Set descriptor heaps

	// Set root signature
	p_commandList->SetGraphicsRootSignature(m_pRootSignature->GetRootSignature());

}

void Renderer::SubmitDrawPacket(const std::vector<DrawPacket>& drawPackets)
{
	m_drawPacketsThisFrame.clear();
	m_drawPacketsThisFrame = drawPackets;
}

void Renderer::SubmitCameraInfo(const CameraInfo& cameraInfo)
{
	m_cameraInfoThisFrame = cameraInfo;
}

//平行光源情報を設定
void Renderer::SubmitDirectionalLight(const DirectionalLight& light)
{
	m_directionalLight = light;	//平行光源情報を保存
}

//一時描画リストの描画(ワールド座標用)
void Renderer::RenderScene(ID3D12GraphicsCommandList* p_commandList)
{
	PSOKey compare{};

	for (size_t i = 0; i < m_drawPacketsThisFrame.size(); i++)
	{
		//パイプラインステートオブジェクトの設定
		if (i == 0)
		{
			auto pso = GetPipelineStateObject(m_drawPacketsThisFrame[i].materialDesc.psoKey);	//パイプラインステートを取得
			p_commandList->SetPipelineState(pso->GetPipelineState());					//パイプラインステートをセット
			compare = m_drawPacketsThisFrame[i].materialDesc.psoKey;
		}
		else if (compare != m_drawPacketsThisFrame[i].materialDesc.psoKey)
		{
			auto pso = GetPipelineStateObject(m_drawPacketsThisFrame[i].materialDesc.psoKey);	//パイプラインステートを取得
			p_commandList->SetPipelineState(pso->GetPipelineState());					//パイプラインステートをセット
		}

		compare = m_drawPacketsThisFrame[i].materialDesc.psoKey;

		// フレームごとのCBVプールを必要数まで確保
		if (i >= m_objectCBWorld.size())
		{
			//新しい定数バッファを作成
			auto newCb = std::make_unique<ConstantBuffer>(m_pDevice, sizeof(PerObjectConstants));

			if (!newCb->GetIsValid())
			{//作成失敗時
				OutputDebugStringA("ConstantBuffer creation failed\n");
				break;
			}

			//プールに追加
			m_objectCBWorld.push_back(std::move(newCb));
		}

		//オブジェクト用定数バッファの取得
		ConstantBuffer* cb = m_objectCBWorld[i].get();
		auto* ptr = cb->GetPtr<PerObjectConstants>();

		//定数バッファに transform を書く（各オブジェクト専用のメモリ）
		ptr->worldMatrix = m_drawPacketsThisFrame[i].worldMatrix;	//ワールド行列
		ptr->worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, ptr->worldMatrix)); //ワールド逆転置行列
		ptr->viewMatrix = m_cameraInfoThisFrame.viewMatrix;					//ビュー行列
		ptr->projMatrix = m_cameraInfoThisFrame.projMatrix;					//プロジェクション行列
		ptr->objectColor = m_drawPacketsThisFrame[i].color;	//オブジェクトの色
		ptr->uvRect = XMFLOAT4(0, 0, 1, 1);		//UV矩形
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
		auto meshGPU = m_drawPacketsThisFrame[i].meshDesc.gpuHandle;

		//セットアップ
		auto vbv = meshGPU->GetVertexBuffer()->GetView();						//頂点バッファビューの取得
		auto ibv = meshGPU->GetIndexBuffer()->GetView();						//インデックスバッファビューの取得
		p_commandList->SetGraphicsRootConstantBufferView(0, cb->GetAddress());	//ルートパラメータ0に定数バッファをセット
		p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());			//プリミティブトポロジの設定
		p_commandList->IASetVertexBuffers(0, 1, &vbv);							//頂点バッファの設定
		p_commandList->IASetIndexBuffer(&ibv);									//インデックスバッファの設定

		//SRVの設定
		uint32_t idx = m_pTextureManager->GetTextureSrvIndex(m_drawPacketsThisFrame[i].materialDesc.textureHandle);
		auto gpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(idx);
		p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

		//描画コマンドの発行
		p_commandList->DrawIndexedInstanced(	//描画コマンド
			meshGPU->GetIndexCount(),						//インデックス数
			1,												//インスタンス数
			m_drawPacketsThisFrame[i].meshDesc.startIndex,	//スタートインデックス位置
			m_drawPacketsThisFrame[i].meshDesc.baseVertex,	//ベース頂点位置
			0												//スタートインスタンス位置
		);
	}
}

// Draw post-process effects
void Renderer::RenderFullScreenPass(ID3D12GraphicsCommandList* p_commandList, RenderTargetTexture* input)
{
	// Set the pipeline state object for post-processing
	auto pso = GetPipelineStateObject(m_postProcessKey);		// Get the pipeline state object for post-processing
	p_commandList->SetPipelineState(pso->GetPipelineState());	// Set the pipeline state for post-processing

	// Set SRV for post-processing
	auto srvIndex = input->GetSrvIndex();	// Get the SRV index for the input render target
	auto gpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(srvIndex);
	p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

	// Reset vertex/index buffers
	D3D12_VERTEX_BUFFER_VIEW nullVBV{};
	p_commandList->IASetVertexBuffers(0, 1, &nullVBV);
	p_commandList->IASetIndexBuffer(nullptr);

	// Draw a full-screen triangle for post-processing
	p_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);	// Set the primitive topology
	p_commandList->DrawInstanced(3, 1, 0, 0);									// Issue draw command (full-screen triangle)
}

//PSO取得
PipelineState* Renderer::GetPipelineStateObject(PSOKey key)
{
	auto it = m_psoMap.find(key);	//PSOマップから検索
	if (it != m_psoMap.end())
	{//見つかった場合はそれを返す
		return it->second.get();
	}
	else
	{//見つからなかった場合は新規作成
		PipelineState* pso = CreatePipelineStateObject(key).get();

		if (!pso)
		{//作成失敗時はデフォルトPSOを返す
			return m_pDefaultPSO.get();
		}

		return pso;
	}
}

//PSO生成
std::shared_ptr<PipelineState> Renderer::CreatePipelineStateObject(const PSOKey& key)
{
	std::shared_ptr<PipelineState> pso = nullptr;

	// Get shaders
	auto vs = m_pShaderLibrary->GetVS(key.vsKey.fileID, key.vsKey.entryID, key.vsKey.defines, key.commonDefines);
	auto ps = m_pShaderLibrary->GetPS(key.psKey.fileID, key.psKey.entryID, key.psKey.defines, key.commonDefines);
	if (!vs || !ps)
	{
		OutputDebugStringA("Shader blob missing\n");
		return nullptr;
	}

	// Create a new pipeline state object
	pso = std::make_shared<PipelineState>(m_pDevice);
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
		return nullptr;
	}

	m_psoMap.emplace(key, pso);
	return pso;
}

// Set up post-process PSO key
void Renderer::PreparePostProcessKey()
{
	PSOKey key{};
	key.vsKey.fileID = VS_FILE_ID::Basic;
	key.vsKey.entryID = VS_ENTRY_ID::PostEffect;
	key.vsKey.defines = {};
	key.psKey.fileID = PS_FILE_ID::Basic;
	key.psKey.entryID = PS_ENTRY_ID::PostEffect;
	key.psKey.defines = {};
	key.blend = BlendMode::Opaque;
	key.depth = DepthMode::Disable;
	key.cull = CullMode::Back;
	key.rtvFormat = RenderTargetFormat::LDR;

	m_postProcessKey = key;
}
