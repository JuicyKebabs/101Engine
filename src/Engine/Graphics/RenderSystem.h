#pragma once
#include "Engine/Graphics/FrameRenderData.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Resource/Texture.h"
#include "Engine/Component/Camera.h"

// Render system class
class RenderSystem
{
public:
	struct SortKeyOpaque
	{
		PSOKey psoKey = {};
	};

	struct SortKeyTransparent
	{
		PSOKey psoKey = {};
		float depth = 0.0f;	// Depth for sorting transparent objects (greater depth first)
	};

	struct FrameSortData
	{
		std::vector<SortKeyOpaque> opaqueKeys;				// Sort keys for opaque objects
		std::vector<SortKeyTransparent> transparentKeys;	// Sort keys for transparent objects

		uint64_t AddOpaqueKey(SortKeyOpaque key) {
			opaqueKeys.push_back(key);
			return opaqueKeys.size() - 1;
		}

		SortKeyOpaque GetOpaqueKey(uint64_t index) const {
			return opaqueKeys[index];
		}

		uint64_t AddTransparentKey(SortKeyTransparent key) {
			transparentKeys.push_back(key);
			return transparentKeys.size() - 1;
		}

		SortKeyTransparent GetTransparentKey(uint64_t index) const {
			return transparentKeys[index];
		}

		void Clear() {
			opaqueKeys.clear();
			transparentKeys.clear();
		}
	};

public:
	RenderSystem() = default;	// Constructor
	~RenderSystem() = default;	// Destructor

	void Register(MeshRenderer* renderer);						// Register a mesh renderer to be rendered
	void Register(SpriteRenderer* renderer);					// Register a sprite renderer to be rendered
	void Unregister(MeshRenderer* renderer);					// Unregister a mesh renderer (stop rendering it)
	void Unregister(SpriteRenderer* renderer);					// Unregister a sprite renderer (stop rendering it)
	void BuildFrameRenderData(const CameraInfo& cameraInfo);	// Build draw packets from the registered mesh renderers

	FrameRenderData& GetFrameRenderData() { return m_frameRenderData; }	// Get the render data for the current frame (contains draw packets and other rendering information)

private:
	std::vector<MeshRenderer*> m_meshRenderers;		// List of mesh renderers in the scene
	std::vector<SpriteRenderer*> m_spriteRenderers;	// List of sprite renderers in the scene
	FrameRenderData m_frameRenderData;				// Render data for the current frame (contains draw packets and other rendering information)
	FrameSortData m_frameSortData;					// Sort data for the current frame (contains sort keys for sorting draw packets)
	CameraInfo m_cameraInfo;						// Cached camera information for the current frame (used for sorting transparent objects)

private:
	MeshRenderItem CreateMeshRenderItem(const SubmeshRenderTemplate& renderTemplate, const MeshRendererProxy& renderProxy);			// Create a draw packet from a sort entry
	SpriteRenderItem CreateSpriteRenderItem(const SpriteRenderTemplate& renderTemplate, const SpriteRendererProxy& renderProxy);	// Create a sprite draw packet from a sort entry

	void SortOpaque();		// Sort opaque draw packets
	void SortTransparent();	// Sort transparent draw packets
	
	RenderQueue GetRenderQueue(const PSOKey& psoKey);			// Determine the render queue for sort entry 
	void NormalizePSOKey(PSOKey& psoKey, RenderQueue queue);	// Normalize the draw packet data

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
	//static inline bool BindLess(const SortData& a, const SortData& b)
	//{
	//	// Convert pointer to integer for comparison
	//	auto ap = reinterpret_cast<std::uintptr_t>(a.gpuHandle);
	//	auto bp = reinterpret_cast<std::uintptr_t>(b.gpuHandle);
	//	// Compare by textureHandle, pMeshGPU, startIndex, baseVertex
	//	return std::tie(a.textureHandle, ap, a.startIndex, a.baseVertex)
	//		< std::tie(b.textureHandle, bp, b.startIndex, b.baseVertex);
	//}
	// Opaque objects sorting
	static inline bool OpaqueLess(const SortKeyOpaque& a, const SortKeyOpaque& b)
	{
		//If PSOKey is the same, sort by bind
		if (a.psoKey == b.psoKey)
		{
			//return BindLess(a, b);
			return false;
		}

		//Otherwise, sort by PSOKey
		return PSOKeyLess(a.psoKey, b.psoKey);
	}
	// Transparent objects sorting (back to front)
	static inline bool TransparentLess(const SortKeyTransparent& a, const SortKeyTransparent& b)
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
			const int64_t bucketA = static_cast<int64_t>(std::floor(a.depth * 64.0f));
			const int64_t bucketB = static_cast<int64_t>(std::floor(b.depth * 64.0f));

			//First, sort by bucket (greater bucket first)
			if (bucketA != bucketB) return bucketA > bucketB;

			//Then, sort by fine depth within the bucket (greater fine depth first)
			const int64_t fineA = (int64_t)std::llround(a.depth * 4096.0f);
			const int64_t fineB = (int64_t)std::llround(b.depth * 4096.0f);
			if (fineA != fineB) return fineA > fineB;

			return false;	// Ignore same depth
		}

		if (a.psoKey != b.psoKey)
		{// If PSOKey is different, sort by PSOKey
			return PSOKeyLess(a.psoKey, b.psoKey);
		}

		//// Finally, sort by bind
		//return BindLess(a, b);

		return false;
	}
};