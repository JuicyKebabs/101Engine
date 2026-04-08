#pragma once
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Resource/Texture.h"
#include "Engine/Component/Camera.h"

struct DrawPacket
{
	MeshDesc meshDesc;			// Mesh data and rendering settings
	MaterialDesc materialDesc;	// Material information and rendering settings
	Matrix4x4 worldMatrix = {};	// World matrix for this draw packet
	Vector4 color{ 1,1,1,1 };	// Color for rendering
};

// Render system class
class RenderSystem
{
public:
	struct SortData
	{
		MeshGPU* gpuHandle;				// GPU handle for the mesh (used for sorting)
		UINT startIndex;				// Start index for drawing (used for sorting)
		UINT baseVertex;				// Base vertex for drawing (used for sorting)
		TextureHandle textureHandle;	// Texture handle for this material (used for sorting)
		PSOKey psoKey;					// Pipeline State Object key for sorting
		float sortDepth = 0.0f;			// Depth value for sorting (used for transparent objects)
		RenderQueue renderQueue;		// Render queue for this draw packet (opaque or transparent)
	};

	struct SortEntry
	{
		SubmeshRenderTemplate renderTemplate;	// Render template for this material
		MeshRenderProxy renderProxy;			// Cached render proxy for this material
		SortData sortData;						// Sort data for this material
	};

public:
	RenderSystem() = default;	// Constructor
	~RenderSystem() = default;	// Destructor

	void Register(MeshRenderer* renderer);					// Register a mesh renderer to be rendered
	void Unregister(MeshRenderer* renderer);				// Unregister a mesh renderer (stop rendering it)
	void BuildDrawPackets(const CameraInfo& cameraInfo);	// Build draw packets from the registered mesh renderers

	const std::vector<DrawPacket>& GetDrawPackets() const { return m_drawPackets; }	// Get the list of draw packets to be processed

private:
	std::vector<MeshRenderer*> m_meshRenderers;	// List of mesh renderers in the scene
	std::vector<DrawPacket> m_drawPackets;		// List of draw packets to be processed

private:
	std::vector<RenderSystem::SortEntry> CreateSortEntriesFromRenderer(MeshRenderer& renderer, const CameraInfo& cameraInfo);			// Create sort entries from a mesh renderer
	RenderQueue GetRenderQueue(const PSOKey& psoKey);								// Determine the render queue for sort entry 
	void NormalizeSortEntry(RenderSystem::SortEntry& sortEntry);				// Normalize the draw packet data
	void SortEntries(std::vector<RenderSystem::SortEntry>& sortEntries);	// Sort draw packets
	SortData CreateSortData(const SubmeshRenderTemplate& submeshTemplate, const MeshRenderProxy& renderProxy, const CameraInfo& cameraInfo);	// Create sort data from a sort entry
	void CreateDrawPacketsFromSortEntries(std::vector<RenderSystem::SortEntry>& sortEntries);			// Create the draw packets

	// Calculate depth as the distance along the camera's forward vector
	static inline float CalculateDepth(const Vector3& position, const CameraInfo& cameraInfo)
	{
		return (position - cameraInfo.position).Dot(cameraInfo.forward);
	}

	// Sorting functions 
	// PSOKey comparison
	static inline bool PSOKeyLess(const PSOKey& a, const PSOKey& b)
	{
		return std::tie(a.vsKey.fileID, a.vsKey.entryID, a.vsKey.defines, a.psKey.fileID, a.psKey.entryID, a.psKey.defines, a.commonDefines, a.blend, a.depth, a.cull)
			< std::tie(b.vsKey.fileID, b.vsKey.entryID, b.vsKey.defines, b.psKey.fileID, b.psKey.entryID, b.psKey.defines, b.commonDefines, b.blend, b.depth, b.cull);
	}
	// Bind sort comparison
	static inline bool BindLess(const SortData& a, const SortData& b)
	{
		// Convert pointer to integer for comparison
		auto ap = reinterpret_cast<std::uintptr_t>(a.gpuHandle);
		auto bp = reinterpret_cast<std::uintptr_t>(b.gpuHandle);
		// Compare by textureHandle, pMeshGPU, startIndex, baseVertex
		return std::tie(a.textureHandle, ap, a.startIndex, a.baseVertex)
			< std::tie(b.textureHandle, bp, b.startIndex, b.baseVertex);
	}
	// Opaque objects sorting
	static inline bool OpaqueLess(const SortData& a, const SortData& b)
	{
		//If PSOKey is the same, sort by bind
		if (a.psoKey == b.psoKey)
		{
			return BindLess(a, b);
		}

		//Otherwise, sort by PSOKey
		return PSOKeyLess(a.psoKey, b.psoKey);
	}
	// Transparent objects sorting (back to front)
	static inline bool TransparentLess(const SortData& a, const SortData& b)
	{
		// Determine if the blend mode is order-dependent
		auto isOrderDependent = [](BlendMode blendMode) {
			return blendMode == BlendMode::Alpha;
			};

		// First, sort by whether the blend mode is order-dependent
		const bool aOrderDependent = isOrderDependent(a.psoKey.blend);
		const bool bOrderDependent = isOrderDependent(b.psoKey.blend);
		if (aOrderDependent != bOrderDependent)
		{// Order-dependent blends first
			return aOrderDependent > bOrderDependent;
		}

		if (aOrderDependent)
		{// For order-dependent blends, sort by depth (greater depth first)
			// Quantize depth to avoid precision issues
			const int64_t bucketA = static_cast<int64_t>(std::floor(a.sortDepth * 64.0f));
			const int64_t bucketB = static_cast<int64_t>(std::floor(b.sortDepth * 64.0f));

			//First, sort by bucket (greater bucket first)
			if (bucketA != bucketB) return bucketA > bucketB;

			//Then, sort by fine depth within the bucket (greater fine depth first)
			const int64_t fineA = (int64_t)std::llround(a.sortDepth * 4096.0f);
			const int64_t fineB = (int64_t)std::llround(b.sortDepth * 4096.0f);
			if (fineA != fineB) return fineA > fineB;

			return false;	// Ignore same depth
		}

		if (a.psoKey != b.psoKey)
		{// If PSOKey is different, sort by PSOKey
			return PSOKeyLess(a.psoKey, b.psoKey);
		}

		// Finally, sort by bind
		return BindLess(a, b);
	}
};