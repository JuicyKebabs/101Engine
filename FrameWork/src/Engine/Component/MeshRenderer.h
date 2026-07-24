#pragma once
#include <optional>
#include "Engine/Component/RendererComponent.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Core/GUID/Guid.h"

//---------------------------------------------------------
// MeshRenderer class
// A component for rendering static meshes in the scene
// ---------------------------------------------------------

struct MeshRendererProxy
{
	CommonRendererProxy common;	// Common render proxy data (position, world matrix, color, visibility)
};

// MeshRendererComponent Class (for static mesh rendering)
class MeshRenderer : public RendererComponent
{
public:
	struct ParamDesc
	{
		std::vector<SubmeshRenderTemplate> templates;	// Render templates for each mesh to be drawn
		Vector4 color{ 1,1,1,1 };						// Color for rendering (can be used to tint the rendered object)
		bool visible = true;							// Visibility flag for this renderer
		std::string name = "MeshRenderer";				// Component name (optional, can be used for debugging or identification)
	};

public:
	MeshRenderer() = default;
	~MeshRenderer() = default;
	void SetParams(const ParamDesc& desc) {
		m_templates = desc.templates;
		m_meshAssetId = {};
		m_pendingMeshAssetId.reset();
		SetColor(desc.color);
		SetVisible(desc.visible);
		SetName(desc.name);
		m_isProxyDirty = true;
	}

	// Set mesh asset for this renderer through AssetManager
	// For de-serialization and inspector manipulation, this function
	bool SetMeshAsset(const Guid& assetId);

	// Getters
	const std::vector<SubmeshRenderTemplate>& GetRenderTemplates() const { return m_templates; }
	const MeshRendererProxy& GetRenderProxy();
	bool IsConfigured() const override { return !m_templates.empty(); }
	const Guid& GetAssetId() const { return m_meshAssetId; }

	// Serialization and deserialization methods
	bool Serialize(nlohmann::json& outJson) const override;
	bool Deserialize(const nlohmann::json& json) override;
	bool ResolveReferences(SceneBase& scene) override;

private:
	std::vector<SubmeshRenderTemplate> m_templates;	// Render templates for each mesh to be drawn
	MeshRendererProxy m_proxy;						// Cached render proxy for this component
	Guid m_meshAssetId;									// Mesh asset ID for this renderer
	std::optional<Guid> m_pendingMeshAssetId;			// Optional pending asset ID for deferred loading (used during deserialization)
private:
	// Override functions for component lifecycle
	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDestroyOverride() override;

	void RebuildRenderProxy();		// Rebuild the render proxy (Called when GetRenderProxy is called and the transform is dirty)
};