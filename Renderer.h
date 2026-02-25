#pragma once
#include "Engine.h"
#include "ComPtr.h"
#include "VertexBuffer.h"
#include "ConstantBuffer.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "IndexBuffer.h"
#include "SharedStruct.h"
#include "RenderData.h"
#include "ShaderLibrary.h"
#include <unordered_map>
#include <vector>
#include <tuple>
#include <cstdint>

// Forward declaration
class TextureManager;
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
		CameraInfo* pInfo,
		TextureManager* textureManager
	);
	void Update(UINT currentBackBufferIndex, CameraInfo& info);	// Update
	void Draw(ID3D12GraphicsCommandList* pCommandList, RENDER_TARGET_TYPE targetType);	// Draw
	// Render list management functions
	void BeginFrame(UINT backIndex);

	// Add render information to the render list
	void SubmitToWorldList(const WorldRenderModel& info);				// World space
	void SubmitToScreenList(const  WorldRenderModel& info);				// Screen space

	void SubmitDirectionalLight(const DirectionalLight& light);	// Directional light information

private:
	RootSignature* m_pRootSignature = nullptr;		// Root signature
	ID3D12Device* m_pDevice = nullptr;				// Device
	CameraInfo* m_cameraInfo = nullptr;				// Camera information structure
	TextureManager* m_pTextureManager = nullptr;	// Texture manager

	// Pipeline State Object related
	std::unordered_map<PSOKey, PipelineState*, PSOKeyHash> m_psoMap;	// PSO map
	PipelineState* m_pDefaultPSO = nullptr;								// Default PSO
	ShaderLibrary* m_pShaderLibrary = nullptr;							// Shader library

	// Temporary render lists for sorting and post-processing
	std::vector<WorldRenderInfo> m_tempWorldRenderList;					// Temporary world render list for sorting
	std::vector<WorldRenderInfo> m_tempScreenRenderList;				// Temporary world render list for sorting

	// Frame-specific object CBV pool (1 object = 1 constant buffer)
	std::vector<ConstantBuffer*> m_objectCBWorldPostProcess;					// For post-processing world space
	std::vector<ConstantBuffer*> m_objectCBWorld[Engine::FRAME_BUFFER_COUNT];	// For world space
	std::vector<ConstantBuffer*> m_objectCBScreen[Engine::FRAME_BUFFER_COUNT];	// For screen space
	UINT m_currBackIndex = 0;

	// Camera matrices
	DirectX::XMMATRIX m_worldView{};	// View matrix for world space
	DirectX::XMMATRIX m_worldProj{};	// Projection matrix for world space
	DirectX::XMMATRIX m_screenProj{};	// Projection matrix for screen space
	DirectX::XMMATRIX m_screenView{};	// View matrix for screen space

	// Lighting information
	DirectionalLight m_directionalLight{};	// Directional light

	// Post-processing related
	PSOKey m_postProcessKey;	// Post-processing PSO key

private:
	void DrawTempRenderListWorld(	// Draw for screen space render list
		ID3D12GraphicsCommandList* p_commandList	// Command list
	);
	void DrawTempRenderListScreen(	// Draw for screen space render list
		ID3D12GraphicsCommandList* p_commandList	// Command list
	);
	void DrawPostProcess(	// Post-processing draw
		ID3D12GraphicsCommandList* p_commandList	// Command list
	);

	DirectX::XMMATRIX CalcBillBoard(const WorldRenderInfo& info);	// Billboard calculation
	float CalcSortDepth(const DirectX::XMFLOAT3& position);	// Sort depth calculation

	PipelineState* GetPipelineStateObject(PSOKey key);				// Get pipeline state object(if not exists, create it)
	PipelineState* CreatePipelineStateObject(const PSOKey& key);	// Create pipeline state object
	void SortRenderListWorldByPSO(std::vector<WorldRenderInfo>& renderList);	// Sort render list by PSO
	void SortRenderListScreenByPSO(std::vector<WorldRenderInfo>& renderList);	// Sort render list by PSO
	void NormalizeKeyForRenderQueueWorld(WorldRenderInfo& info);	// Normalize PSO key for render queue
	void NormalizeKeyForRenderQueueScreen(WorldRenderInfo& info);	// Normalize PSO key for render queue
	void PreparePostProcessKey();	// Prepare post-processing information

	// Sorting functions
	// PSOKey comparison
	static inline bool PSOKeyLess(const PSOKey& a, const PSOKey& b)
	{
		return std::tie(a.vsKey.fileID, a.vsKey.entryID, a.vsKey.defines, a.psKey.fileID, a.psKey.entryID, a.psKey.defines, a.commonDefines, a.blend, a.depth, a.cull)
			< std::tie(b.vsKey.fileID, b.vsKey.entryID, b.vsKey.defines, b.psKey.fileID, b.psKey.entryID, b.psKey.defines, b.commonDefines, b.blend, b.depth, b.cull);
	}
	// Bind sort comparison
	static inline bool BindLess(const WorldRenderInfo& a, const WorldRenderInfo& b)
	{
		// Convert pointer to integer for comparison
		auto ap = reinterpret_cast<std::uintptr_t>(a.common.pMeshGPU);
		auto bp = reinterpret_cast<std::uintptr_t>(b.common.pMeshGPU);
		// Compare by srvIndex, pMeshGPU, startIndex, baseVertex
		return std::tie(a.common.srvIndex, ap, a.startIndex, a.baseVertex)
			< std::tie(b.common.srvIndex, bp, b.startIndex, b.baseVertex);
	}
	// Opaque objects sorting
	static inline bool OpaqueLess(const WorldRenderInfo& a, const WorldRenderInfo& b)
	{
		//If PSOKey is the same, sort by bind
		if (a.common.psoKey == b.common.psoKey)
		{
			return BindLess(a, b);
		}

		//Otherwise, sort by PSOKey
		return PSOKeyLess(a.common.psoKey, b.common.psoKey);
	}
	// Transparent objects sorting (back to front)
	static inline bool TransparentLess(const WorldRenderInfo& a, const WorldRenderInfo& b)
	{
		// Determine if the blend mode is order-dependent
		auto isOrderDependent = [](BLEND_MODE blendMode) {
			return blendMode == BLEND_ALPHA;
			};

		// First, sort by whether the blend mode is order-dependent
		const bool aOrderDependent = isOrderDependent(a.common.psoKey.blend);
		const bool bOrderDependent = isOrderDependent(b.common.psoKey.blend);
		if (aOrderDependent != bOrderDependent)
		{// Order-dependent blends first
			return aOrderDependent > bOrderDependent;
		}

		if (aOrderDependent)
		{// For order-dependent blends, sort by depth (greater depth first)
			// Quantize depth to avoid precision issues
			const int64_t bucketA = static_cast<int64_t>(std::floor(a.common.sortDepth * 64.0f));
			const int64_t bucketB = static_cast<int64_t>(std::floor(b.common.sortDepth * 64.0f));

			//First, sort by bucket (greater bucket first)
			if (bucketA != bucketB) return bucketA > bucketB;

			//Then, sort by fine depth within the bucket (greater fine depth first)
			const int64_t fineA = (int64_t)std::llround(a.common.sortDepth * 4096.0f);
			const int64_t fineB = (int64_t)std::llround(b.common.sortDepth * 4096.0f);
			if (fineA != fineB) return fineA > fineB;

			return false;	// Ignore same depth
		}

		if (a.common.psoKey != b.common.psoKey)
		{// If PSOKey is different, sort by PSOKey
			return PSOKeyLess(a.common.psoKey, b.common.psoKey);
		}

		// Finally, sort by bind
		return BindLess(a, b);
	}
};