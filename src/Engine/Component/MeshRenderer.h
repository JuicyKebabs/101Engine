#pragma once
#include <memory>
#include "Component.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Graphics/RenderTemplateFactory.h"

// Forward declaration
class MeshManager;
class TextureManager;

// Render proxy structure for a draw packet
// This structure contains all the necessary dynamic information
struct MeshRendererProxy
{
	Vector3 position{};			// Position for this draw packet
	Matrix4x4 worldMatrix = {};	// World matrix for this draw packet
	Vector4 color{ 1,1,1,1 };	// Color for rendering
	bool visible = true;		// Visibility flag for this draw packet
};

// MeshRendererComponent Class (for static mesh rendering)
class MeshRenderer : public Component
{
public:
	MeshRenderer(const std::string& name = "MeshRenderer") : Component(name) {}
	~MeshRenderer() = default;

	void OnStart() override;
	void PreUpdate(float deltaTime) override;
	void Update(float deltaTime) override;
	void LateUpdate(float deltaTime) override;
	void OnDestroy() override;
	void Flush();

	void SetColor(const Vector4& color) { m_color = color; m_isProxyDirty = true; }
	void SetVisibility(bool visible) { m_isVisible = visible; m_isProxyDirty = true; }

	Vector4 GetColor() const { return m_color; }
	bool IsVisible() const { return m_isVisible; }
	bool IsConfigured() const { return m_isConfigured; }

	const std::vector<SubmeshRenderTemplate>& GetRenderTemplates() const { return m_templates; }
	const MeshRendererProxy& GetRenderProxy();

	void Initialize(std::vector<SubmeshRenderTemplate> templates);	// Initialize the render templates

private:
	std::vector<SubmeshRenderTemplate> m_templates;	// Render templates for each mesh to be drawn (contains mesh data, texture info, and rendering settings)
	MeshRendererProxy m_proxy;						// Cached render proxy for this component (used for rendering)
	Vector4 m_color{ 1,1,1,1 };						// Color for rendering (can be used to tint the mesh)
	bool m_isVisible = true;						// Visibility flag for the mesh
	bool m_isConfigured = false;					// Flag to indicate if the render templates have been configured (used to prevent rendering before initialization)

	bool m_isProxyDirty = true;			// Flag to indicate if the render proxy needs to be updated
	uint64_t m_transformGeneration = 0;	// Generation counter for the transform

private:
	void RebuildRenderProxy();		// Rebuild the render proxy (Called when GetRenderProxy is called and the transform is dirty)
	void CheckIfTransformChanged();	// Check if the transform has changed since the last frame and mark the render proxy as dirty if it has
};