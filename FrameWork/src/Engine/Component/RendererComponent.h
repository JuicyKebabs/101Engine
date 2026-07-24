#pragma once
#include "Component.h"
#include "Engine/Core/Math/Math.h"

//-------------------------------------------------------
// RendererComponent class
// Base class for all renderer components in the engine
// ------------------------------------------------------

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
	RendererComponent() = default;
	~RendererComponent() = default;

	// Flush function to be called before rendering
	void Flush() { CheckIfTransformChanged(); }

	// Setters
	void SetVisible(bool visible) { m_isVisible = visible; m_isProxyDirty = true; }
	void SetColor(const Vector4& color) { m_color = color; m_isProxyDirty = true; }

	// Getters
	Vector4 GetColor() const { return m_color; }
	virtual bool IsVisible() const { return m_isVisible; }
	virtual bool IsConfigured() const { return false; }	// Check if the renderer has been configured with necessary resources

	// Serialization and deserialization methods
	bool Serialize(nlohmann::json& outJson) const override;
	bool Deserialize(const nlohmann::json& json) override;

protected:
	Vector4 m_color{ 1,1,1,1 };		// Color for rendering (can be used to tint the rendered object)
	bool m_isVisible = true;		// Visibility flag for this renderer

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