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


// Forward declaration
class TextureManager;
struct CameraInfo;
enum class RENDER_TARGET_TYPE;

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
	void Initialize(											// Initialization
		ID3D12Device* pDevice,
		TextureManager* textureManager
	);
	void Update(UINT currentBackBufferIndex, const CameraInfo& info);	// Update
	void Render(ID3D12GraphicsCommandList* pCommandList, RENDER_TARGET_TYPE targetType);	// Draw
	// Render list management functions
	void BeginFrame(UINT backIndex);

	// Add render information to the render list
	//void SubmitToWorldList(const WorldRenderModel& info);				// World space
	//void SubmitToScreenList(const  WorldRenderModel& info);				// Screen space

	void SubmitDrawPacket(const std::vector<DrawPacket>& drawPackets);	// Submit draw packets
	void SubmitCameraInfo(const CameraInfo& cameraInfo);				// Submit camera information for this frame
	void SubmitDirectionalLight(const DirectionalLight& light);	// Directional light information

private:
	RootSignature* m_pRootSignature = nullptr;		// Root signature
	ID3D12Device* m_pDevice = nullptr;				// Device
	TextureManager* m_pTextureManager = nullptr;	// Texture manager

	// Pipeline State Object related
	std::unordered_map<PSOKey, PipelineState*, PSOKeyHash> m_psoMap;	// PSO map
	PipelineState* m_pDefaultPSO = nullptr;								// Default PSO
	std::unique_ptr<ShaderLibrary> m_pShaderLibrary = nullptr;			// Shader library

	// Temporary render lists for sorting and post-processing
	//std::vector<WorldRenderInfo> m_tempWorldRenderList;					// Temporary world render list for sorting
	//std::vector<WorldRenderInfo> m_tempScreenRenderList;				// Temporary world render list for sorting

	std::vector<DrawPacket> m_drawPacketsThisFrame;	// Draw packets submitted this frame (for post-processing)
	CameraInfo m_cameraInfoThisFrame;				// Camera information for this frame (for post-processing)


	// Frame-specific object CBV pool (1 object = 1 constant buffer)
	std::vector<std::unique_ptr<ConstantBuffer> > m_objectCBWorldPostProcess;					// For post-processing world space
	std::vector<std::unique_ptr<ConstantBuffer>> m_objectCBWorld[Engine::FRAME_BUFFER_COUNT];	// For world space
	std::vector<std::unique_ptr<ConstantBuffer>> m_objectCBScreen[Engine::FRAME_BUFFER_COUNT];	// For screen space
	UINT m_currBackIndex = 0;

	// Lighting information
	DirectionalLight m_directionalLight{};	// Directional light

	// Post-processing related
	PSOKey m_postProcessKey;	// Post-processing PSO key

private:
	void RenderTempPackets(	// Draw for world space render list
		ID3D12GraphicsCommandList* p_commandList	// Command list
	);
	void RenderPostProcess(	// Post-processing draw
		ID3D12GraphicsCommandList* p_commandList	// Command list
	);

	//float CalcSortDepth(const Vector3& position);	// Sort depth calculation

	PipelineState* GetPipelineStateObject(PSOKey key);				// Get pipeline state object(if not exists, create it)
	PipelineState* CreatePipelineStateObject(const PSOKey& key);	// Create pipeline state object
	void PreparePostProcessKey();	// Prepare post-processing information
};