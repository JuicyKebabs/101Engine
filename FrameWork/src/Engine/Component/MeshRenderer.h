#pragma once
#include "Engine/Component/RendererComponent.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Core/GUID/Guid.h"

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
		SetColor(desc.color);
		SetVisible(desc.visible);
		SetName(desc.name);
	}

	// Set mesh asset for this renderer through AssetManager
	// For de-serialization and inspector manipulation, this function
	void SetAsset(const Guid& assetId);

	// Getters
	const std::vector<SubmeshRenderTemplate>& GetRenderTemplates() const { return m_templates; }
	const MeshRendererProxy& GetRenderProxy();
	bool IsConfigured() const override { return !m_templates.empty(); }
	const Guid& GetAssetId() const { return m_assetId; }

private:
	std::vector<SubmeshRenderTemplate> m_templates;	// Render templates for each mesh to be drawn
	MeshRendererProxy m_proxy;						// Cached render proxy for this component
	Guid m_assetId;									// Mesh asset ID for this renderer

private:
	// Override functions for component lifecycle
	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDestroyOverride() override;

	void RebuildRenderProxy();		// Rebuild the render proxy (Called when GetRenderProxy is called and the transform is dirty)
};