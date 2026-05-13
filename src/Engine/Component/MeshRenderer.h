#pragma once
#include "Engine/Component/RendererComponent.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Graphics/RenderTemplateFactory.h"

struct MeshRendererProxy
{
	CommonRendererProxy common;	// Common render proxy data (position, world matrix, color, visibility)
};

// MeshRendererComponent Class (for static mesh rendering)
class MeshRenderer : public RendererComponent
{
public:
	struct InitDesc : public RendererComponent::InitDesc
	{
		std::vector<SubmeshRenderTemplate> templates;
		InitDesc(std::string name = "MeshRenderer") : RendererComponent::InitDesc(Vector4(1,1,1,1), true, name) {}
		InitDesc(const std::vector<SubmeshRenderTemplate>& templates, Vector4 color, bool visible, const std::string& name = "MeshRenderer")
			: RendererComponent::InitDesc(color, visible, name), templates(templates) {}
	};

public:
	MeshRenderer() = default;
	~MeshRenderer() = default;
	void Init(const InitDesc& desc) {
		m_templates = desc.templates;
		RendererComponent::Init(desc);
	}

	// Getters
	const std::vector<SubmeshRenderTemplate>& GetRenderTemplates() const { return m_templates; }
	const MeshRendererProxy& GetRenderProxy();

private:
	std::vector<SubmeshRenderTemplate> m_templates;	// Render templates for each mesh to be drawn (contains mesh data, texture info, and rendering settings)
	MeshRendererProxy m_proxy;						// Cached render proxy for this component (used for rendering)

private:
	// Override functions for component lifecycle
	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDestroyOverride() override;

	void RebuildRenderProxy();		// Rebuild the render proxy (Called when GetRenderProxy is called and the transform is dirty)
};