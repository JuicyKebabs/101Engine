#pragma once
#include <unordered_map>
#include <memory>
#include <vector>
#include <tuple>
#include <cstdint>
#include <d3dx12.h>
#include "Engine/Engine.h"
#include "Engine/Core/ComPtr/ComPtr.h"
#include "VertexBuffer.h"
#include "ConstantBuffer.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "IndexBuffer.h"
#include "Engine/Core/Utility/SharedStruct.h"
#include "RenderData.h"
#include "ShaderLibrary.h"
#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Graphics/FrameRenderData.h"
#include "Engine/Graphics/DescriptorHeapAllocator.h"


// Forward declaration
struct CameraInfo;

// Renderer class
class Renderer
{
public:
	Renderer() {};	// Constructor
	~Renderer();	// Destructor

	// Get singleton instance
	static Renderer* GetInstance()
	{
		static Renderer instance;
		return &instance;
	}

	// Main processing functions
	void Initialize(ID3D12Device* pDevice, DescriptorHeapAllocator* pDescriptorHeapAllocator, TextureManager* pTextureManager);	// Initialization
	void Update(UINT currentBackBufferIndex, const CameraInfo& info);															// Update
	// Render list management functions
	void BeginFrame(ID3D12GraphicsCommandList* p_commandList);											// Begin frame (set root signature, descriptor heaps, etc.)
	void RenderScene(ID3D12GraphicsCommandList* p_commandList);											// Render the scene using submitted draw packets
	void RenderFullScreenPass(ID3D12GraphicsCommandList* p_commandList, RenderTargetTexture* input);	// Render a full-screen pass (for post-processing)

	void SubmitFrameRenderData(const FrameRenderData& frameRenderData);	// Submit draw packets
	void SubmitCameraInfo(const CameraInfo& cameraInfo);				// Submit camera information for this frame
	void SubmitDirectionalLight(const DirectionalLight& light);			// Directional light information

private:
	std::unique_ptr<RootSignature> m_pRootSignature = nullptr;		// Root signature
	ID3D12Device* m_pDevice = nullptr;								// Device
	DescriptorHeapAllocator* m_pDescriptorHeapAllocator = nullptr;	// Descriptor heap allocator
	TextureManager* m_pTextureManager = nullptr;					// Texture manager

	// Pipeline State Object related
	std::unordered_map<PSOKey, std::shared_ptr<PipelineState>, PSOKeyHash> m_psoMap;	// PSO map
	std::shared_ptr<PipelineState> m_pDefaultPSO = nullptr;								// Default PSO
	std::unique_ptr<ShaderLibrary> m_pShaderLibrary = nullptr;			// Shader library

	FrameRenderData m_frameRenderData;	// Render data for this frame (contains draw packets and other rendering information)
	CameraInfo m_cameraInfoThisFrame;	// Camera information for this frame (for post-processing)

	// Constant buffers for per-frame and per-draw data
	std::unique_ptr<ConstantBuffer> m_frameCB;
	std::unique_ptr<ConstantBuffer> m_lightCB;
	std::vector<std::unique_ptr<ConstantBuffer>> m_meshCBWorld;
	std::vector<std::unique_ptr<ConstantBuffer>> m_spriteCBWorld;

	// Lighting information
	DirectionalLight m_directionalLight{};	// Directional light

	// Post-processing related
	PSOKey m_postProcessKey;	// Post-processing PSO key

private:
	void RenderMesh(ID3D12GraphicsCommandList* p_commandList, const MeshRenderItem& item, int itemIndex, PSOKey& compare);		// Render a mesh
	void RenderSprite(ID3D12GraphicsCommandList* p_commandList, const SpriteRenderItem& item, int itemINdex, PSOKey& compare);	// Render a sprite


	PipelineState* GetPipelineStateObject(PSOKey key);				// Get pipeline state object(if not exists, create it)
	std::shared_ptr<PipelineState> CreatePipelineStateObject(const PSOKey& key);	// Create pipeline state object
	void PreparePostProcessKey();	// Prepare post-processing information
};