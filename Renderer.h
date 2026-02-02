#pragma once
#include "Engine.h"
#include "ComPtr.h"
#include "VertexBuffer.h"
#include "ConstantBuffer.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "IndexBuffer.h"
#include "TextureManager.h"
#include "SharedStruct.h"
#include "RenderData.h"

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
		CameraInfo* pInfo);
	void Update(UINT currentBackBufferIndex, CameraInfo& info);	// Update
	void Draw(													// Draw
		UINT index,
		ID3D12GraphicsCommandList* commandList,
		TextureManager& textureManager
	);

	// Render list management functions
	void BeginFrame(UINT backIndex);

	// Add render information to the render list
	void SubmitToWorldList(const WorldRenderModel& info);		// World space
	void SubmitToEffectList(const  EffectRenderModel& info);	// Effect
	void SubmitToScreenList(const  WorldRenderModel& info);		// Screen space

	void SubmitDirectionalLight(const DirectionalLight& light);	// Directional light information

private:
	RootSignature* m_pRootSignature = nullptr;			// Root signature
	PipelineState* m_pPipelineStateWorldNoLight[BLEND_MAX]{};	// Pipeline state object for world space (without lighting)
	PipelineState* m_pPipelineStateWorldLight[BLEND_MAX]{};		// Pipeline state object for world space (with lighting)
	PipelineState* m_pPipelineStateEffect[BLEND_MAX]{};			// Pipeline state object for effect
	PipelineState* m_pPipelineStateScreen[BLEND_MAX]{};			// Pipeline state object for screen space

	ID3D12Device* m_pDevice = nullptr;	// Device
	CameraInfo* m_cameraInfo = nullptr;	// Camera information structure

	std::vector<WorldRenderInfo> m_drawListWorldNoLight[BLEND_MAX]{};	// Draw list (world space without lighting)
	std::vector<WorldRenderInfo> m_drawListWorldLight[BLEND_MAX]{};		// Draw list (world space with lighting)
	std::vector<EffectRenderInfo> m_drawListEffect[BLEND_MAX]{};		// Draw list (effect)
	std::vector<WorldRenderInfo> m_drawListScreen[BLEND_MAX]{};			// Draw list (screen space)

	// Frame-specific object CBV pool (1 object = 1 constant buffer)
	std::vector<ConstantBuffer*> m_objectCBWorld[Engine::FRAME_BUFFER_COUNT];	// For world space
	std::vector<ConstantBuffer*> m_objectCBEffect[Engine::FRAME_BUFFER_COUNT];	// For effect
	std::vector<ConstantBuffer*> m_objectCBScreen[Engine::FRAME_BUFFER_COUNT];	// For screen space
	UINT m_currBackIndex = 0;

	// Camera matrices
	DirectX::XMMATRIX m_worldView{};	// View matrix for world space
	DirectX::XMMATRIX m_worldProj{};	// Projection matrix for world space
	DirectX::XMMATRIX m_screenProj{};	// Projection matrix for screen space
	DirectX::XMMATRIX m_screenView{};	// View matrix for screen space

	// Lighting information
	DirectionalLight m_directionalLight{};	// Directional light

private:
	// Drawing function for the render list
	void DrawRenderListWorld(	// Draw for world space render list
		ID3D12GraphicsCommandList* p_commandList,	// Command list
		TextureManager& textureManager				// Texture manager class
	);
	void DrawRenderListEffect(	// Draw for effect render list
		ID3D12GraphicsCommandList* p_commandList,	// Command list
		TextureManager& textureManager				// Texture manager class
	);
	void DrawRenderListScreen(	// Draw for screen space render list
		ID3D12GraphicsCommandList* p_commandList,	// Command list
		TextureManager& textureManager				// Texture manager class
	);

	// Render list sorting functions
	void SortDrawList();			// Sort render list
	void SortDrawListOpaque();		// Sort opaque object render list
	void SortDrawListTransparent();	// Sort transparent object render list

	DirectX::XMMATRIX CalcBillBoard(const WorldRenderInfo& info);	// Billboard calculation
};