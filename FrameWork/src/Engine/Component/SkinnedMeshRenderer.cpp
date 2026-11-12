#include "SkinnedMeshRenderer.h"

#include "Engine/Actor/Actor.h"
#include "Engine/Animation/SkinningPalette.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Scene/SceneBase.h"

#include <algorithm>
#include <utility>

void SkinnedMeshRenderer::OnAttachOverride()
{
	Actor* actor = GetOwner();
	SceneBase* scene = actor ? actor->GetOwner() : nullptr;

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
}

bool SkinnedMeshRenderer::SetMeshAsset(const Guid& assetId)
{
	if (!assetId.IsValid())
	{
		m_meshAssetId = {};
		m_pendingMeshAssetId.reset();
		m_submeshes.clear();
		m_hasAnimationPose = false;
		m_isProxyDirty = true;
		return true;
	}

	Actor* owner = GetOwner();
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;
	const EngineContext* context = GetEngineContext();

	if (!scene || !context || !context->pAssetManager)
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

	std::vector<SkinnedSubmeshRenderTemplate> submeshes;
	submeshes.reserve(model.placements.size());
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

		const auto& skin = model.meshSkins[*placement.meshIndex];
		SkinnedSubmeshRenderTemplate submesh;

		if (!skin.valid ||
			!animation::skinning::BuildPalette(skin.bones, bindPose, placement.localToModel, submesh.palette))
		{
			DBG("SkinnedMeshRenderer: Could not prepare a mesh palette.");
			return false;
		}

		hasSkinnedPlacement |= !submesh.palette.empty();
		submesh.bindPalette = submesh.palette;
		submesh.renderTemplate = PrepareSubmeshRenderTemplate(*context, *scene, placement, false);

		if (submesh.renderTemplate.materialDesc.handle == rendering::MaterialHandle{})
		{
			DBG("SkinnedMeshRenderer: Could not prepare a mesh material.");
			return false;
		}

		submeshes.push_back(std::move(submesh));
	}

	if (!hasSkinnedPlacement)
	{
		DBG("SkinnedMeshRenderer: Model has no skinned placements.");
		return false;
	}

	acquisition.Commit();

	m_meshAssetId = assetId;
	m_pendingMeshAssetId.reset();
	m_submeshes.swap(submeshes);
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

	// Reset each submesh's palette to its bind pose
	// (first element of the palette is root-to-model transform for the mesh).
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