#include "Renderer.h"
#include <algorithm>
#include <vector>
#include "App/App.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Graphics/ShaderLibrary.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/Debug/Debug.h"

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

void Renderer::SubmitFrameRenderData(const FrameRenderData& frameRenderData)
{
	m_frameRenderData = frameRenderData;
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

// Render all items in this frame's render data
void Renderer::RenderScene(ID3D12GraphicsCommandList* p_commandList)
{
	// Allocate constant buffers for this frame
	size_t totalItems = m_frameRenderData.GetOpaqueCount() + m_frameRenderData.GetTransparentCount();
	if (m_objectCBWorld.size() < totalItems) 
	{
		size_t difference = totalItems - m_objectCBWorld.size();
		for (size_t i = 0; i < difference; i++)
		{
			auto newCb = std::make_unique<ConstantBuffer>(m_pDevice, sizeof(PerObjectConstants));
			m_objectCBWorld.push_back(std::move(newCb));
		}
	}

	PSOKey compare{};
	int itemIndex = 0; // Index to track constant buffer usage

	for (auto& item : m_frameRenderData.opaque)
	{
		switch (item.renderType)
		{
		case RenderType::Mesh:
			RenderMesh(p_commandList, m_frameRenderData.GetMesh(item.handle), itemIndex, compare);
			break;
		case RenderType::Sprite:
			RenderSprite(p_commandList, m_frameRenderData.GetSprite(item.handle), itemIndex, compare);
			break;
		default:
			break;
		}

		itemIndex++;
	}

	for (auto& item : m_frameRenderData.transparent)
	{
		switch (item.renderType)
		{
		case RenderType::Mesh:
			RenderMesh(p_commandList, m_frameRenderData.GetMesh(item.handle), itemIndex, compare);
			break;
		case RenderType::Sprite:
			RenderSprite(p_commandList, m_frameRenderData.GetSprite(item.handle), itemIndex, compare);
			break;
		default:
			break;
		}

		itemIndex++;
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

void Renderer::RenderMesh(ID3D12GraphicsCommandList* p_commandList, const MeshRenderItem& item, int itemIndex, PSOKey& compare)
{
	// Check if the Constant buffer is valid
	if (itemIndex >= m_objectCBWorld.size())
	{
		DBG("Not enough constant buffers allocated\n");
		return;
	}
	if (!m_objectCBWorld[itemIndex]->GetIsValid())
	{
		DBG("Constant buffer is not valid\n");
		return;
	}

	// Compare PSO keys to minimize state changes (optional optimization)
	PSOKey currentKey = item.materialDesc.psoKey;
	if (currentKey != compare)
	{
		auto pso = GetPipelineStateObject(currentKey);				// Get the pipeline state object for this item
		p_commandList->SetPipelineState(pso->GetPipelineState());	// Set the pipeline state for this item
		compare = currentKey;										// Update the compare key
	}


	// Set up the constant buffer for this mesh
	auto ptr = m_objectCBWorld[itemIndex]->GetPtr<PerObjectConstants>();
	ptr->worldMatrix = item.worldMatrix;
	ptr->worldInvTranspose = item.worldMatrix.Transpose();
	ptr->viewMatrix = m_cameraInfoThisFrame.viewMatrix;
	ptr->projMatrix = m_cameraInfoThisFrame.projMatrix;
	ptr->objectColor = item.color;
	ptr->uvRect = XMFLOAT4(0, 0, 1, 1);
	ptr->lightDir_Intensity = Vector4(
		m_directionalLight.direction.x,
		m_directionalLight.direction.y,
		m_directionalLight.direction.z,
		m_directionalLight.intensity
	);
	ptr->lightColor_Ambient = Vector4(
		m_directionalLight.color.x,
		m_directionalLight.color.y,
		m_directionalLight.color.z,
		m_directionalLight.ambient
	);

	// Set mesh data
	auto meshGPU = item.meshDesc.gpuHandle;
	auto vbv = meshGPU->GetVertexBuffer()->GetView();
	auto ibv = meshGPU->GetIndexBuffer()->GetView();
	p_commandList->SetGraphicsRootConstantBufferView(0, m_objectCBWorld[itemIndex]->GetAddress());
	p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());
	p_commandList->IASetVertexBuffers(0, 1, &vbv);
	p_commandList->IASetIndexBuffer(&ibv);

	// Set SRV for the texture
	int32_t idx = m_pTextureManager->GetTextureSrvIndex(item.materialDesc.textureHandle);
	auto gpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(idx);
	p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

	// Draw command
	p_commandList->DrawIndexedInstanced(
		meshGPU->GetIndexCount(),
		1,
		item.meshDesc.startIndex,
		item.meshDesc.baseVertex,
		0
	);
}

void Renderer::RenderSprite(ID3D12GraphicsCommandList* p_commandList, const SpriteRenderItem& item, int itemIndex, PSOKey& compare)
{
	// Check if the Constant buffer is valid
	if (itemIndex >= m_objectCBWorld.size())
	{
		DBG("Not enough constant buffers allocated\n");
		return;
	}
	if (!m_objectCBWorld[itemIndex]->GetIsValid())
	{
		DBG("Constant buffer is not valid\n");
		return;
	}

	// Compare PSO keys to minimize state changes (optional optimization)
	PSOKey currentKey = item.materialDesc.psoKey;
	if (currentKey != compare)
	{
		auto pso = GetPipelineStateObject(currentKey);				// Get the pipeline state object for this item
		p_commandList->SetPipelineState(pso->GetPipelineState());	// Set the pipeline state for this item
		compare = currentKey;										// Update the compare key
	}

	// Set up the constant buffer for this mesh
	auto ptr = m_objectCBWorld[itemIndex]->GetPtr<PerObjectConstants>();
	ptr->worldMatrix = item.worldMatrix;
	ptr->worldInvTranspose = item.worldMatrix.Transpose();
	ptr->viewMatrix = m_cameraInfoThisFrame.viewMatrix;
	ptr->projMatrix = m_cameraInfoThisFrame.projMatrix;
	ptr->objectColor = item.color;
	ptr->uvRect = Vector4(
		item.uvOffset.x,
		item.uvOffset.y,
		item.uvOffset.x + item.uvScale.x,
		item.uvOffset.y + item.uvScale.y
	);
	ptr->lightDir_Intensity = Vector4(
		m_directionalLight.direction.x,
		m_directionalLight.direction.y,
		m_directionalLight.direction.z,
		m_directionalLight.intensity
		);
	ptr->lightColor_Ambient = Vector4(
		m_directionalLight.color.x,
		m_directionalLight.color.y,
		m_directionalLight.color.z,
		m_directionalLight.ambient
	);

	// Set SRV for the texture
	int32_t idx = m_pTextureManager->GetTextureSrvIndex(item.materialDesc.textureHandle);
	auto gpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(idx);
	p_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

	// Draw command (assuming a full-screen quad for sprites)
	p_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	p_commandList->DrawInstanced(6, 1, 0, 0); // Draw a quad (2 triangles)
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