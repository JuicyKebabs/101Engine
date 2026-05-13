#pragma once
#include "Component.h"

// Common render proxy structure for draw packets (Specific renderer component has this structure with additional fields as needed)
struct CommonRendererProxy
{
	Vector3 position{};			// Position for this draw packet
	Matrix4x4 worldMatrix = {};	// World matrix for this draw packet
	Vector4 color{ 1,1,1,1 };	// Color for rendering
	bool visible = true;		// Visibility flag for this draw packet
};

// Base RendererComponent Class (for common rendering properties and functionality)
class RendererComponent : public Component
{
public:
	// Structure for initializing
	struct InitDesc : public Component::InitDesc
	{
		Vector4 color{ 1,1,1,1 };	// Color for rendering (can be used to tint the rendered object)
		bool visible = true;		// Visibility flag for this renderer
		InitDesc(const Vector4& color = Vector4(1,1,1,1), bool visible = true, const std::string& name = "RendererComponent")
			: Component::InitDesc(name), color(color), visible(visible) {}
	};

public:
	RendererComponent() = default;
	~RendererComponent() = default;

	// Initialize function by InitDesc
	void Init(const InitDesc& desc) {
		m_color = desc.color;
		m_isVisible = desc.visible;
		Component::Init(desc);
	}

	// Flush function to be called before rendering
	void Flush() { CheckIfTransformChanged(); }

	// Setters
	void SetVisible(bool visible) { m_isVisible = visible; m_isProxyDirty = true; }
	void SetColor(const Vector4& color) { m_color = color; m_isProxyDirty = true; }

	// Getters
	virtual bool IsVisible() const { return m_isVisible; }
	Vector4 GetColor() const { return m_color; }

protected:
	Vector4 m_color{ 1,1,1,1 };		// Color for rendering (can be used to tint the rendered object)
	bool m_isVisible = true;		// Visibility flag for this renderer
	bool m_isConfigured = false;	// Flag to check if the renderer has been configured with necessary data (e.g., render templates)

	uint64_t m_transformGeneration = 0;	// Cached generation of the transform to detect changes
	bool m_isProxyDirty = true;			// Flag to indicate if the render proxy needs to be rebuilt due to transform changes

protected:
	// Override functions for component lifecycle
	virtual void OnStartOverride() override = 0;
	virtual void PreUpdateOverride(float deltaTime) override = 0;
	virtual void UpdateOverride(float deltaTime) override = 0;
	virtual void LateUpdateOverride(float deltaTime) override = 0;
	virtual void OnDestroyOverride() override = 0;

	// Function to check if the transform has changed and mark the proxy as dirty if needed
	void CheckIfTransformChanged();
};