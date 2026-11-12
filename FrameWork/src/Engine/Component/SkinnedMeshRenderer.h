#pragma once

#include "Engine/Component/RendererComponent.h"
#include "Engine/Resource/AssetReference.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Graphics/RenderTemplateFactory.h"

#include <span>
#include <optional>

//------------------------------------------------------------------------------
// SkinnedMeshRenderer
// MeshRenderer component that supports skinned meshes. 
// It can be used to render animated characters with AnimationPlayer component.
//------------------------------------------------------------------------------

struct SkinnedModelView;

struct SkinnedSubmeshRenderTemplate
{
	SubmeshRenderTemplate renderTemplate;
	std::vector<Matrix4x4> palette;
	std::vector<Matrix4x4> bindPalette;
};

class SkinnedMeshRenderer : public RendererComponent
{
public:
	SkinnedMeshRenderer() { SetCastShadow(true); }

	bool SetMeshAsset(const Guid& assetId);
	Guid GetAssetId() const { return m_meshAssetId; }
	bool IsConfigured() const override { return !m_submeshes.empty(); }
	const std::vector<SkinnedSubmeshRenderTemplate>& GetSubmeshes() const { return m_submeshes; }

	bool ApplyAnimationPose(const SkinnedModelView& model, std::span<const Matrix4x4> nodeToModel);
	void ResetToBindPose() noexcept;
	bool HasAnimationPose() const { return m_hasAnimationPose; }

	AssetReference<MeshAsset> GetMeshAssetReference() const;

	bool TrySetMeshAssetReference(const AssetReference<MeshAsset>& value);
	bool ResolveReferences(SceneBase& scene) override;

private:
	Guid m_meshAssetId;
	std::optional<Guid> m_pendingMeshAssetId;
	bool m_hasAnimationPose = false;

	// Same order as the Scene-retained model placements.
	std::vector<SkinnedSubmeshRenderTemplate> m_submeshes;

	void OnAttachOverride() override;
	void OnDetachOverride() override;
	void OnStartOverride() override {}
	void PreUpdateOverride(float deltaTime) override {}
	void UpdateOverride(float deltaTime) override {}
	void LateUpdateOverride(float deltaTime) override {}
	void OnDestroyOverride() override {}
};