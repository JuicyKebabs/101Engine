#include "SkinnedMeshRenderer.h"

#include "Engine/Actor/Actor.h"
#include "Engine/Animation/SkinningPalette.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/RenderAssetResources.h"
#include "Engine/Scene/SceneBase.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace
{

void DestroySubmeshResources(
	rendering::Renderer& renderer,
	std::span<const SkinnedSubmeshRenderTemplate> submeshs) noexcept
{
	for (const auto& submesh : submeshs)
	{
		const auto material = submesh.renderTemplate.materialDesc.handle;

		// Destroy material.
		if (material != rendering::MaterialHandle{})
		{
			const auto destroyed = renderer.DestroyMaterial(material);

			if (!destroyed.IsSuccess())
			{
				DBG("SkinnedMeshRenderer: Material cleanup: %s", destroyed.message.c_str());
			}
		}

		// Destroy buffer for uploading skinning palette.
		if (submesh.paletteBuffer != rendering::BufferHandle{})
		{
			const auto destroyed = renderer.DestroyStructuredBuffer(submesh.paletteBuffer);

			if (!destroyed.IsSuccess())
			{
				DBG("SkinnedMeshRenderer: Palette Buffer cleanup: %s", destroyed.message.c_str());
			}
		}
	}
}

// Helper class to manage the submeshs wich are being registered to the renderer.
// It ensures that the resources are properly released if any error occurs during the registration process.
// The PendingSubmeshes object should be used as a temporary container for the submeshes
struct PendingSubmeshes
{
	explicit PendingSubmeshes(rendering::Renderer& owner) : renderer(owner) {}
	~PendingSubmeshes() noexcept
	{
		// Release the resources of the submeshes automatically
		// when the PendingSubmeshes object is destroyed.
		DestroySubmeshResources(renderer, submeshes);
	}

	PendingSubmeshes(const PendingSubmeshes&) = delete;
	PendingSubmeshes& operator=(const PendingSubmeshes&) = delete;

	rendering::Renderer& renderer;
	std::vector<SkinnedSubmeshRenderTemplate> submeshes;
};

rendering::Result RegisterSubmeshResources(
	rendering::Renderer& renderer,
	const RenderAssetResources& assets,
	SkinnedSubmeshRenderTemplate& submesh)
{
	auto& material = submesh.renderTemplate.materialDesc;

	if (!submesh.bindPalette.empty())
	{
		static_assert(sizeof(Matrix4x4) == 64);

		if (submesh.bindPalette.size() > (std::numeric_limits<std::uint32_t>::max)())
		{
			return { false, "RegisterSubmeshResources: Palette size exceeds 32-bit limit." };
		}

		// Create a structured buffer for uploading the skinning palette to the GPU.
		rendering::StructuredBufferDesc bufferDesc;
		bufferDesc.elementCount = static_cast<std::uint32_t>(submesh.bindPalette.size());
		bufferDesc.elementStride = sizeof(Matrix4x4);
		
		const auto buffer = renderer.CreateStructuredBuffer(bufferDesc);

		if (!buffer.result.IsSuccess())
		{
			return { false, "RegisterSubmeshResources: Palette buffer creation failed: " + buffer.result.message, buffer.result.failureKind };
		}

		submesh.paletteBuffer = buffer.handle;

		// Update the buffer with the bind pose palette.
		auto initialized =
			renderer.UpdateStructuredBuffer(
				buffer.handle,
				std::as_bytes(std::span(submesh.bindPalette)));

		if (!initialized.IsSuccess())
		{
			return { false, "RegisterSubmeshResources: Palette buffer initial update is failed: " + initialized.message, initialized.failureKind };
		}

		// Set the material's pipeline and shadow pipeline to the built-in SkinnedMeshRenderer pipelines.
		material.description.pipeline = assets.SkinnedMeshPipeline();
		material.description.shadowPipeline = assets.SkinnedShadowPipeline();

		// Add a material parameter for the skinning palette buffer.
		// Renderer can binds the appropriate buffer with this parameter when drawing the submesh.
		rendering::MaterialParameter paletteParam;
		paletteParam.name = "gBoneMatrices";
		paletteParam.value = submesh.paletteBuffer;

		material.description.parameters.push_back(std::move(paletteParam));
	}

	// Register the material with the renderer.
	const auto registered = renderer.RegisterMaterial(material.description);
	material.handle = registered.handle;

	return registered.result;
}

} // namespace

SkinnedMeshRenderer::~SkinnedMeshRenderer()
{
	ReleaseSubmeshes();
}

void SkinnedMeshRenderer::ReleaseSubmeshes() noexcept
{
	if (m_resourceRenderer)
	{
		DestroySubmeshResources(*m_resourceRenderer, m_submeshes);
		m_resourceRenderer = nullptr;
	}
	
	m_submeshes.clear();
}

void SkinnedMeshRenderer::OnAttachOverride()
{
	Actor* actor = GetOwner();
	SceneBase* scene = actor ? actor->GetOwner() : nullptr;

	// Register this component with the scene's render system if the scene.
	if (scene && !IsDestroyed())
	{
		scene->GetRenderSystem()->Register(this);
	}
}

void SkinnedMeshRenderer::OnDetachOverride()
{
	Actor* actor = GetOwner();
	SceneBase* scene = actor ? actor->GetOwner() : nullptr;

	if (scene)
	{
		scene->GetRenderSystem()->Unregister(this);
	}

	ReleaseSubmeshes();
	m_hasAnimationPose = false;

}

bool SkinnedMeshRenderer::SetMeshAsset(const Guid& assetId)
{
	if (!assetId.IsValid())
	{
		m_meshAssetId = {};
		m_pendingMeshAssetId.reset();
		m_hasAnimationPose = false;
		m_isProxyDirty = true;

		// It is necessary to unregister the buffers and
		// materials of the submeshes from the renderer.
		ReleaseSubmeshes();

		return true;
	}

	Actor* owner = GetOwner();
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;
	const EngineContext* context = GetEngineContext();

	if (!scene || !context || !context->pAssetManager)
	{
		return false;
	}

	if (!context->pRenderer || !context->pRenderAssets)
	{
		return false;
	}

	// Resolve mesh asset.
	AssetManager& assetManager = *context->pAssetManager;
	const AssetEntry* entry = assetManager.GetAssetEntry(assetId);

	if (!entry || entry->type != AssetType::Mesh)
	{
		return false;
	}

	SceneAssetAcquisition acquisition(assetManager, *scene);
	const SkinnedModelView model = assetManager.GetSkinnedModel(*scene, assetId);

	// Build bind pose for given model.
	std::vector<Matrix4x4> bindPose;

	if (!animation::skinning::BuildBindPose(model.nodes, bindPose))
	{
		DBG("SkinnedMeshRenderer: Model has no valid bind hierarchy.");
		return false;
	}

	// Create submeshs in the temporary PendingSubmeshes object
	// not to leave any failed submeshes in the component if any error occurs.
	PendingSubmeshes pending(*context->pRenderer);
	pending.submeshes.reserve(model.placements.size());
	bool hasSkinnedPlacement = false;

	// Construct SkinnedSubmeshRenderTemplate from GeometryInfo.
	for (const GeometryInfo& placement : model.placements)
	{
		if (!placement.meshIndex ||
			*placement.meshIndex >= model.meshSkins.size() ||
			placement.geometry == rendering::GeometryHandle{})
		{
			DBG("SkinnedMeshRenderer: Model has an unavailable mesh placement.");
			return false;
		}

		// Build a palette for the mesh placement using its skinning data.
		const auto& skin = model.meshSkins[*placement.meshIndex];
		auto& submesh = pending.submeshes.emplace_back();

		if (!skin.valid ||
			!animation::skinning::BuildPalette(skin.bones, bindPose, placement.localToModel, submesh.palette))
		{
			DBG("SkinnedMeshRenderer: Could not prepare a mesh palette.");
			return false;
		}

		hasSkinnedPlacement |= !submesh.palette.empty();
		submesh.bindPalette = submesh.palette;

		// Construct a SubmeshRenderTemplate for the submesh.
		submesh.renderTemplate = MakeSubmeshRenderTemplate(*context, placement, false);
		
		// Register the material and palette buffer for the submesh with the renderer.
		const auto registered = RegisterSubmeshResources(*context->pRenderer, *context->pRenderAssets, submesh);

		if (!registered.IsSuccess()) 
		{
			DBG("SkinnedMeshRenderer: Submesh resources: %s", registered.message.c_str());
			return false;
		}
	}

	if (!hasSkinnedPlacement)
	{
		DBG("SkinnedMeshRenderer: Model has no skinned placements.");
		return false;
	}

	acquisition.Commit();

	// Release the previous submeshes and their resources from the renderer.
	ReleaseSubmeshes();

	// Update the information of this component
	// with the new mesh asset and its submeshes.
	m_resourceRenderer = context->pRenderer;
	m_submeshes.swap(pending.submeshes);
	m_meshAssetId = assetId;
	m_pendingMeshAssetId.reset();
	m_hasAnimationPose = false;
	m_isProxyDirty = true;

	return true;
}

bool SkinnedMeshRenderer::ApplyAnimationPose(
	const SkinnedModelView& model,
	std::span<const Matrix4x4> nodeToModel)
{
	if (model.placements.size() != m_submeshes.size())
	{
		return false;
	}

	// Build a palette for each mesh placement in the model.
	std::vector<std::vector<Matrix4x4>> palettes(model.placements.size());

	for (std::size_t index = 0; index < model.placements.size(); ++index)
	{
		const auto& placement = model.placements[index];

		if (!placement.meshIndex || *placement.meshIndex >= model.meshSkins.size())
		{
			return false;
		}

		// Build a palette for the mesh placement using its skinning data
		// and the provided node-to-model transforms.
		const auto& skin = model.meshSkins[*placement.meshIndex];

		if (!skin.valid ||
			skin.bones.size() != m_submeshes[index].bindPalette.size() ||
			!animation::skinning::BuildPalette(skin.bones, nodeToModel, placement.localToModel, palettes[index]))
		{
			return false;
		}
	}

	for (std::size_t index = 0; index < m_submeshes.size(); ++index)
	{
		m_submeshes[index].palette.swap(palettes[index]);
	}

	m_hasAnimationPose = true;
	return true;
}

void SkinnedMeshRenderer::ResetToBindPose() noexcept
{
	if (!m_hasAnimationPose)
	{
		return;
	}

	// Restore the complete CPU palette. RenderSystem uploads it before drawing.
	for (auto& submesh : m_submeshes)
	{
		std::copy(submesh.bindPalette.begin(), submesh.bindPalette.end(), submesh.palette.begin());
	}

	m_hasAnimationPose = false;
}

AssetReference<MeshAsset> SkinnedMeshRenderer::GetMeshAssetReference() const
{
	AssetReferenceValue reference;
	reference.guid = m_pendingMeshAssetId.value_or(m_meshAssetId);
	reference.expectedType = AssetType::Mesh;
	reference.resolved = m_meshAssetId.IsValid();

	AssetReference<MeshAsset> value;
	value.SetValue(reference);
	return value;
}

bool SkinnedMeshRenderer::TrySetMeshAssetReference(const AssetReference<MeshAsset>& value)
{
	if (!value.HasValue())
	{
		return SetMeshAsset(Guid{});
	}

	const EngineContext* context = GetEngineContext();

	// Set guid if the asset has already been registered and resolved in the catalog.
	if (value.IsResolved() && context && context->pAssetManager)
	{
		const AssetEntry* entry = context->pAssetManager->GetAssetEntry(value.GetGuid());

		if (entry)
		{
			return SetMeshAsset(value.GetGuid());
		}
	}

	// Otherwise, retain the guid for later resolution 
	// when the asset is registered in the catalog.
	SetMeshAsset(Guid{});
	m_pendingMeshAssetId = value.GetGuid();
	return true;
}

bool SkinnedMeshRenderer::ResolveReferences(SceneBase& scene)
{
	// Resolve only if there is a pending mesh asset id to resolve.
	if (!m_pendingMeshAssetId)
	{
		return true;
	}

	const Actor* owner = GetOwner();
	const EngineContext* context = GetEngineContext();

	if (!owner || owner->GetOwner() != &scene || !context)
	{
		return false;
	}

	if (!context->pAssetManager)
	{
		return false;
	}

	// Try to resolve the pending mesh asset id to a valid mesh asset in the catalog.
	const Guid assetId = *m_pendingMeshAssetId;
	const AssetEntry* entry = context->pAssetManager->GetAssetEntry(assetId);

	if (entry && entry->type != AssetType::Mesh)
	{
		return false;
	}

	// Set its guid.
	if(!entry || !SetMeshAsset(assetId))
	{
		DBG("SkinnedMeshRenderer: Model unavailable. Mesh GUID was retained.");
	}

	return true;
}