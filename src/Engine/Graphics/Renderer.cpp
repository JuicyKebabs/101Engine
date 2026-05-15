#include "Renderer.h"
#include <algorithm>
#include <vector>
#include "App/App.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Graphics/ShaderLibrary.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Window/WindowInfo.h"

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

	m_colorFrameCB = std::make_unique<ConstantBuffer>(m_pDevice, sizeof(FrameConstants));
	m_shadowFrameCB = std::make_unique<ConstantBuffer>(m_pDevice, sizeof(FrameConstants));
	m_lightCB = std::make_unique<ConstantBuffer>(m_pDevice, sizeof(LightConstants));

	//ポストプロセス用PSOキーの設定
	PreparePostProcessKey();
	//シャドウマップ用PSOキーの設定
	PrepareShadowMapKey();
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

// Submit directional Light information
void Renderer::SubmitDirectionalLight(const DirectionalLight& light)
{
	m_directionalLight = light;

	Vector3 dir = light.direction.Normalized();
	Vector3 sceneCenter = Vector3::Zero();
	Vector3 eye = sceneCenter - dir * 80.0f;

	Vector3 up = (std::abs(dir.y) > 0.99f)
		? Vector3(0.0f, 0.0f, 1.0f)
		: Vector3(0.0f, 1.0f, 0.0f);

	m_directionalLight.view = Matrix4x4::CreateLookAt(eye, sceneCenter, up);
	m_directionalLight.proj = Matrix4x4::CreateOrthographic(20.0f, 20.0f, 0.1f, 200.0f);
}

void Renderer::RenderShadowMap(ID3D12GraphicsCommandList* p_commandList)
{
	auto pso = GetPipelineStateObject(m_shadowMapKey);
	p_commandList->SetPipelineState(pso->GetPipelineState());

	// Set frame constants for shadow map rendering
	auto framePtr = m_shadowFrameCB->GetPtr<FrameConstants>();
	framePtr->view = m_directionalLight.view;
	framePtr->proj = m_directionalLight.proj;
	p_commandList->SetGraphicsRootConstantBufferView(0, m_shadowFrameCB->GetAddress());

	int itemIndex = 0;

	for (auto& item : m_frameRenderData.opaque)
	{
		if (item.renderType == RenderType::Mesh) RenderMeshForShadow(p_commandList, m_frameRenderData.GetMesh(item.handle), itemIndex);
		itemIndex++;
	}
}

// Render all items in this frame's render data
void Renderer::RenderScene(ID3D12GraphicsCommandList* p_commandList, uint32_t shadowMapSrvIndex)
{
	// Set frame-level constants
	auto framePtr = m_colorFrameCB->GetPtr<FrameConstants>();
	framePtr->view = m_cameraInfoThisFrame.viewMatrix;
	framePtr->proj = m_cameraInfoThisFrame.projMatrix;
	framePtr->cameraPosition = m_cameraInfoThisFrame.position;
	p_commandList->SetGraphicsRootConstantBufferView(0, m_colorFrameCB->GetAddress());

	// Set lighting constants ( Currently, one directional light is only supported for simplicity)
	auto lightPtr = m_lightCB->GetPtr<LightConstants>();
	lightPtr->lightDir_Intensity = Vector4(
		m_directionalLight.direction.x,
		m_directionalLight.direction.y,
		m_directionalLight.direction.z,
		m_directionalLight.intensity
	);
	lightPtr->lightColor_Ambient = Vector4(
		m_directionalLight.color.x,
		m_directionalLight.color.y,
		m_directionalLight.color.z,
		m_directionalLight.ambient
	);
	p_commandList->SetGraphicsRootConstantBufferView(2, m_lightCB->GetAddress());

	// Set shadow map SRV
	auto shadowGpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(shadowMapSrvIndex);
	p_commandList->SetGraphicsRootDescriptorTable(4, shadowGpuHandle);

	// Allocate constant buffers for this frame
	size_t totalMeshCount = m_frameRenderData.GetMeshCount();
	if (m_meshCB.size() < totalMeshCount)
	{
		size_t toAllocate = totalMeshCount - m_meshCB.size();
		for (size_t i = 0; i < toAllocate; i++)
		{
			m_meshCB.push_back(std::make_unique<ConstantBuffer>(m_pDevice, sizeof(MeshRenderConstants)));
		}
	}

	size_t totalSpriteCount = m_frameRenderData.GetSpriteCount();
	if (m_spriteCB.size() < totalSpriteCount)
	{
		size_t toAllocate = totalSpriteCount - m_spriteCB.size();
		for (size_t i = 0; i < toAllocate; i++)
		{
			m_spriteCB.push_back(std::make_unique<ConstantBuffer>(m_pDevice, sizeof(SpriteRenderConstants)));
		}
	}

	PSOKey compare{};
	int meshItemIndex = 0;
	int spriteItemIndex = 0;

	for (auto& item : m_frameRenderData.opaque)
	{
		switch (item.renderType)
		{
		case RenderType::Mesh:
			RenderMesh(p_commandList, m_frameRenderData.GetMesh(item.handle), meshItemIndex, compare);
			meshItemIndex++;
			break;
		case RenderType::Sprite:
			RenderSprite(p_commandList, m_frameRenderData.GetSprite(item.handle), spriteItemIndex, compare);
			spriteItemIndex++;
			break;
		default:
			break;
		}
	}

	for (auto& item : m_frameRenderData.transparent)
	{
		switch (item.renderType)
		{
		case RenderType::Mesh:
			RenderMesh(p_commandList, m_frameRenderData.GetMesh(item.handle), meshItemIndex, compare);
			meshItemIndex++;
			break;
		case RenderType::Sprite:
			RenderSprite(p_commandList, m_frameRenderData.GetSprite(item.handle), spriteItemIndex, compare);
			spriteItemIndex++;
			break;
		default:
			break;
		}
	}
}

// Draw post-process effects
void Renderer::RenderFullScreenPass(ID3D12GraphicsCommandList* p_commandList, GpuTexture* input)
{
	// Set the pipeline state object for post-processing
	auto pso = GetPipelineStateObject(m_postProcessKey);		// Get the pipeline state object for post-processing
	p_commandList->SetPipelineState(pso->GetPipelineState());	// Set the pipeline state for post-processing

	// Set SRV for post-processing
	auto srvIndex = input->GetSrvIndex();	// Get the SRV index for the input render target
	auto gpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(srvIndex);
	p_commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);

	// Reset vertex/index buffers
	D3D12_VERTEX_BUFFER_VIEW nullVBV{};
	p_commandList->IASetVertexBuffers(0, 1, &nullVBV);
	p_commandList->IASetIndexBuffer(nullptr);

	// Draw a full-screen triangle for post-processing
	p_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);	// Set the primitive topology
	p_commandList->DrawInstanced(3, 1, 0, 0);									// Issue draw command (full-screen triangle)
}

void Renderer::RenderScreenSpace(ID3D12GraphicsCommandList* p_commandList)
{
	// Set orthographic projection for screen space rendering
	auto orthoProj = Matrix4x4::CreateOrthographic(
		WindowInfo::GetInstance().GetWidth(), 
		WindowInfo::GetInstance().GetHeight(), 
		-1.0f, 
		100.0f);

	// Set frame-level constants for screen space rendering
	auto framePtr = m_colorFrameCB->GetPtr<FrameConstants>();
	framePtr->view = Matrix4x4::Identity;
	framePtr->proj = orthoProj;
	framePtr->cameraPosition = m_cameraInfoThisFrame.position;
	p_commandList->SetGraphicsRootConstantBufferView(0, m_colorFrameCB->GetAddress());

	// Allocate constant buffers for ui items
	size_t totalUIItemCount = m_frameRenderData.GetUICount();
	if (m_uiCB.size() < totalUIItemCount)
	{
		size_t toAllocate = totalUIItemCount - m_uiCB.size();
		for (size_t i = 0; i < toAllocate; i++)
		{
			m_uiCB.push_back(std::make_unique<ConstantBuffer>(m_pDevice, sizeof(UIRenderConstants)));
		}
	}

	PSOKey compare{};
	int meshItemIndex = 0;
	int spriteItemIndex = 0;
	int uiItemIndex = 0;

	for(auto& item : m_frameRenderData.screenspace)
	{
		switch (item.renderType)
		{
		case RenderType::Mesh:
			RenderMesh(p_commandList, m_frameRenderData.GetMesh(item.handle), meshItemIndex, compare); // Screen space items can reuse the first constant buffer since they won't be drawn together
			meshItemIndex++;
			break;
		case RenderType::Sprite:
			RenderSprite(p_commandList, m_frameRenderData.GetSprite(item.handle), spriteItemIndex, compare); // Screen space items can reuse the first constant buffer since they won't be drawn together
			spriteItemIndex++;
			break;
		case RenderType::UI:
			RenderUI(p_commandList, m_frameRenderData.GetUI(item.handle), uiItemIndex, compare); // Screen space items can reuse the first constant buffer since they won't be drawn together
			uiItemIndex++;
			break;
		default:
			break;
		}
	}

}

void Renderer::RenderMesh(ID3D12GraphicsCommandList* p_commandList, const MeshRenderItem& item, int itemIndex, PSOKey& compare)
{
	// Check if the Constant buffer is valid
	if (itemIndex >= m_meshCB.size())
	{
		DBG("Not enough constant buffers allocated\n");
		return;
	}
	if (!m_meshCB[itemIndex]->GetIsValid())
	{
		DBG("Constant buffer is not valid\n");
		return;
	}

	// Check if the PSO key's vertex shader file ID is correct
	PSOKey currentKey = item.common.materialDesc.psoKey;
	if (currentKey.vsKey.fileID != VS_FILE_ID::Mesh) {
		currentKey.vsKey.fileID = VS_FILE_ID::Mesh;
	}

	// Compare PSO keys to minimize state changes (optional optimization)
	if (currentKey != compare || itemIndex == 0)
	{
		auto pso = GetPipelineStateObject(currentKey);				// Get the pipeline state object for this item

		p_commandList->SetPipelineState(pso->GetPipelineState());	// Set the pipeline state for this item
		compare = currentKey;										// Update the compare key
	}


	// Set up the constant buffer for this mesh
	auto ptr = m_meshCB[itemIndex]->GetPtr<MeshRenderConstants>();
	ptr->worldMatrix = item.common.worldMatrix;
	ptr->worldInvTranspose = Matrix4x4::Transpose(item.common.worldMatrix.Inverse());
	ptr->lightViewProj = m_directionalLight.view * m_directionalLight.proj;
	ptr->objectColor = item.common.color;
	p_commandList->SetGraphicsRootConstantBufferView(1, m_meshCB[itemIndex]->GetAddress());

	// Set mesh data
	auto meshGPU = item.meshDesc.gpuHandle;
	auto vbv = meshGPU->GetVertexBuffer()->GetView();
	auto ibv = meshGPU->GetIndexBuffer()->GetView();
	p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());
	p_commandList->IASetVertexBuffers(0, 1, &vbv);
	p_commandList->IASetIndexBuffer(&ibv);

	// Set SRV for the texture
	int32_t idx = m_pTextureManager->GetTextureSrvIndex(item.common.materialDesc.textureHandle);
	auto gpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(idx);
	p_commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);

	// Draw command
	p_commandList->DrawIndexedInstanced(
		meshGPU->GetIndexCount(),
		1,
		item.meshDesc.startIndex,
		item.meshDesc.baseVertex,
		0
	);
}

void Renderer::RenderMeshForShadow(ID3D12GraphicsCommandList* p_commandList, const MeshRenderItem& item, int itemIndex)
{
	if(itemIndex >= m_meshForShadowCB.size())
	{
		m_meshForShadowCB.push_back(std::make_unique<ConstantBuffer>(m_pDevice, sizeof(MeshRenderConstants)));
	}

	// Set up the constant buffer for this mesh
	auto ptr = m_meshForShadowCB[itemIndex]->GetPtr<MeshRenderConstants>();
	ptr->worldMatrix = item.common.worldMatrix;
	ptr->worldInvTranspose = Matrix4x4::Transpose(item.common.worldMatrix.Inverse());
	ptr->objectColor = item.common.color;
	p_commandList->SetGraphicsRootConstantBufferView(1, m_meshForShadowCB[itemIndex]->GetAddress());

	// Set mesh data
	auto meshGPU = item.meshDesc.gpuHandle;
	auto vbv = meshGPU->GetVertexBuffer()->GetView();
	auto ibv = meshGPU->GetIndexBuffer()->GetView();
	p_commandList->IASetPrimitiveTopology(meshGPU->GetTopology());
	p_commandList->IASetVertexBuffers(0, 1, &vbv);
	p_commandList->IASetIndexBuffer(&ibv);

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
	if (itemIndex >= m_spriteCB.size())
	{
		DBG("Not enough constant buffers allocated\n");
		return;
	}
	if (!m_spriteCB[itemIndex]->GetIsValid())
	{
		DBG("Constant buffer is not valid\n");
		return;
	}

	// Compare PSO keys to minimize state changes (optional optimization)
	PSOKey currentKey = item.common.materialDesc.psoKey;
	if (currentKey.vsKey.fileID != VS_FILE_ID::Sprite) {
		currentKey.vsKey.fileID = VS_FILE_ID::Sprite;
	}
	if (!currentKey.indexFree) currentKey.indexFree = true;

	if (currentKey != compare || itemIndex == 0)
	{
		auto pso = GetPipelineStateObject(currentKey);				// Get the pipeline state object for this item
		p_commandList->SetPipelineState(pso->GetPipelineState());	// Set the pipeline state for this item
		compare = currentKey;										// Update the compare key
	}

	// Set up the constant buffer for this mesh
	auto ptr = m_spriteCB[itemIndex]->GetPtr<SpriteRenderConstants>();
	ptr->worldMatrix = item.common.worldMatrix;
	ptr->color = item.common.color;
	ptr->uvRect = Vector4(
		item.uvOffset.x,
		item.uvOffset.y,
		item.uvOffset.x + item.uvScale.x,
		item.uvOffset.y + item.uvScale.y
	);
	ptr->pivot = item.pivot;
	ptr->flip = item.flip;
	p_commandList->SetGraphicsRootConstantBufferView(1, m_spriteCB[itemIndex]->GetAddress());

	// Set SRV for the texture
	int32_t idx = m_pTextureManager->GetTextureSrvIndex(item.common.materialDesc.textureHandle);
	auto gpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(idx);
	p_commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);

	// Draw command (assuming a full-screen quad for sprites)
	p_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	p_commandList->DrawInstanced(6, 1, 0, 0); // Draw a quad (2 triangles)
}

void Renderer::RenderUI(ID3D12GraphicsCommandList* p_commandList, const UIRenderItem& item, int itemIndex, PSOKey& compare)
{
	// Check if the Constant buffer is valid
	if (itemIndex >= m_uiCB.size())
	{
		DBG("Not enough constant buffers allocated\n");
		return;
	}
	if (!m_uiCB[itemIndex]->GetIsValid())
	{
		DBG("Constant buffer is not valid\n");
		return;
	}

	// Compare PSO keys to minimize state changes (optional optimization)
	PSOKey currentKey = item.common.materialDesc.psoKey;
	if (currentKey.vsKey.fileID != VS_FILE_ID::Sprite) {
		currentKey.vsKey.fileID = VS_FILE_ID::Sprite;
	}
	if (!currentKey.indexFree) currentKey.indexFree = true;

	if (currentKey != compare || itemIndex == 0)
	{
		auto pso = GetPipelineStateObject(currentKey);				// Get the pipeline state object for this item
		p_commandList->SetPipelineState(pso->GetPipelineState());	// Set the pipeline state for this item
		compare = currentKey;										// Update the compare key
	}

	// Set up the constant buffer for this mesh
	auto ptr = m_uiCB[itemIndex]->GetPtr<UIRenderConstants>();
	ptr->worldMatrix = item.common.worldMatrix;
	ptr->color = item.common.color;
	ptr->uvRect = Vector4(
		item.uvOffset.x,
		item.uvOffset.y,
		item.uvOffset.x + item.uvScale.x,
		item.uvOffset.y + item.uvScale.y
	);
	ptr->pivot = item.pivot;
	ptr->flip = item.flip;
	p_commandList->SetGraphicsRootConstantBufferView(1, m_uiCB[itemIndex]->GetAddress());

	// Set SRV for the texture
	int32_t idx = m_pTextureManager->GetTextureSrvIndex(item.common.materialDesc.textureHandle);
	auto gpuHandle = m_pDescriptorHeapAllocator->GetCbvSrvUavGpuHandle(idx);
	p_commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);

	// Draw command (assuming a full-screen quad for sprites)
	p_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	p_commandList->DrawInstanced(6, 1, 0, 0); // Draw a quad (2 triangles)
}

//PSO取得
PipelineState* Renderer::GetPipelineStateObject(PSOKey key)
{
	// Serch for the PSO in the map
	auto it = m_psoMap.find(key);
	if (it != m_psoMap.end())
	{
		return it->second.get();	// Return
	}

	// Create a new PSO if not found
	if (!CreatePipelineStateObject(key))
	{
		return m_pDefaultPSO.get();	// Return default PSO if creation failed
	}

	return m_psoMap[key].get();
}

//PSO生成
std::shared_ptr<PipelineState> Renderer::CreatePipelineStateObject(const PSOKey& key)
{
	std::shared_ptr<PipelineState> pso = nullptr;

	// Create a new pipeline state object
	pso = std::make_shared<PipelineState>(m_pDevice);
	pso->SetInputLayout(Vertex::InputLayout);
	pso->SetRootSignature(m_pRootSignature->GetRootSignature());

	auto vs = m_pShaderLibrary->GetVS(key.vsKey.fileID, key.vsKey.entryID, key.vsKey.defines, key.commonDefines);
	if (!vs) 
	{
		OutputDebugStringA("Vertex shader blob missing\n");
		return nullptr;
	}
	pso->SetVertexShader(vs.Get());

	if (!key.depthOnly) 
	{
		auto ps = m_pShaderLibrary->GetPS(key.psKey.fileID, key.psKey.entryID, key.psKey.defines, key.commonDefines);
		if (!ps)
		{
			OutputDebugStringA("Pixel shader blob missing\n");
			return nullptr;
		}
		pso->SetPixelShader(ps.Get());
		pso->SetBlendMode(key.blend);
		pso->FreeIndex(key.indexFree);
	}
	else
	{
		pso->SetDepthOnly();
	}

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
	key.vsKey.fileID = VS_FILE_ID::PostEffect;
	key.vsKey.entryID = VS_ENTRY_ID::Main;
	key.vsKey.defines = {};
	key.psKey.fileID = PS_FILE_ID::PostEffect;
	key.psKey.entryID = PS_ENTRY_ID::Main;
	key.psKey.defines = {};
	key.blend = BlendMode::Opaque;
	key.depth = DepthMode::Disable;
	key.cull = CullMode::Back;
	key.rtvFormat = RenderTargetFormat::LDR;
	m_postProcessKey = key;
}

void Renderer::PrepareShadowMapKey()
{
	PSOKey key{};
	key.vsKey.fileID = VS_FILE_ID::ShadowMap;
	key.vsKey.entryID = VS_ENTRY_ID::Main;
	key.depth = DepthMode::TestWrite;
	key.cull = CullMode::Back;
	key.depthOnly = true;
	m_shadowMapKey = key;
}