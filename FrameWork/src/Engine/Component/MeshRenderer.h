#pragma once
#include <optional>
#include "Engine/Resource/AssetReference.h"
#include "Engine/Component/RendererComponent.h"
#include "Engine/Graphics/RenderTemplateFactory.h"

class Transform;

// Proxy borrows no resource; all transforms/colors are CPU values. Geometry
// Bounds/revisions live in this Component's templates, never AssetManager.
struct MeshRendererProxy
{
    CommonRendererProxy common;
};

class MeshRenderer : public RendererComponent
{
public:
    struct ParamDesc
    {
        std::vector<SubmeshRenderTemplate> templates;
        Vector4 color{ 1, 1, 1, 1 };
        bool visible = true;
        std::string name = "MeshRenderer";
    };

    MeshRenderer() { SetCastShadow(true); }
    void SetParams(const ParamDesc& description)
    {
        m_templates = description.templates;
        Guid empty;
        m_meshAssetId = empty;
        m_pendingMeshAssetId.reset();
        SetColor(description.color);
        SetVisible(description.visible);
        SetName(description.name);
        m_isProxyDirty = true;
    }

    bool SetMeshAsset(const Guid& assetId);
    const std::vector<SubmeshRenderTemplate>& GetRenderTemplates() const { return m_templates; }
    const MeshRendererProxy& GetRenderProxy();
    bool IsConfigured() const override { return !m_templates.empty(); }
    AssetReference<MeshAsset> GetMeshAssetReference() const;
    bool TrySetMeshAssetReference(const AssetReference<MeshAsset>& value);
    Guid GetAssetId() const;
    bool ResolveReferences(SceneBase& scene) override;
    bool SynchronizeRenderResources() override;

private:
    struct PreparedMeshAssetState
    {
        Guid assetId;
        std::vector<SubmeshRenderTemplate> templates;
    };

    std::vector<SubmeshRenderTemplate> m_templates;
    MeshRendererProxy m_proxy;
    Guid m_meshAssetId;
    std::optional<Guid> m_pendingMeshAssetId;
    AssetPrepareResult PrepareMeshAssetState(const Guid& assetId, PreparedMeshAssetState& output) const;
    void CommitMeshAssetState(PreparedMeshAssetState&& state);
    bool SetPendingMeshAssetReference(const AssetReference<MeshAsset>& value);
    void OnAttachOverride() override;
    void OnStartOverride() override;
    void PreUpdateOverride(float deltaTime) override;
    void UpdateOverride(float deltaTime) override;
    void LateUpdateOverride(float deltaTime) override;
    void OnDetachOverride() override;
    void OnDestroyOverride() override;
    void RebuildRenderProxy();
    Matrix4x4 BuildWorldMatrix(Transform* transform) const;
};
