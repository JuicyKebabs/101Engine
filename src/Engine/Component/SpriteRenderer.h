#pragma once
#include "Component.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Resource/Texture.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Graphics/RenderTemplateFactory.h"

struct SpriteRendererProxy
{
    Vector3 position{};				// Position for this draw packet
    Matrix4x4 worldMatrix = {};		// World matrix for this draw packet
    Vector4 color{ 1,1,1,1 };		// Color for rendering
	Vector2 uvScale{ 1,1 };			// UV scale for texture mapping
	Vector2 uvOffset{ 0,0 };		// UV offset for texture mapping
	Vector2 pivot{ 0.5f, 0.5f };	// Pivot point for the sprite
	Vector2 flip{ 1,1 };			// Flip flags for X and Y axes (1 for normal, -1 for flipped)
    bool visible = true;			// Visibility flag for this draw packet
};

class SpriteRenderer : public Component
{
public:
    SpriteRenderer(std::string name = "SpriteRenderer") : Component(name) {}
	~SpriteRenderer() {};

	void OnStart() override;
	void PreUpdate(float deltaTime) override;
	void Update(float deltaTime) override;
	void LateUpdate(float deltaTime) override;
	void OnDestroy() override;
	void Flush();

	void Initialize(const SpriteRenderTemplate& renderTemplate) { m_template = renderTemplate; m_isConfigured = true; m_isProxyDirty = true; }

	void SetColor(const Vector4& color) { m_color = color; m_isProxyDirty = true; }
	void SetUVScale(const Vector2& uvScale) { m_uvScale = uvScale; m_isProxyDirty = true; }
	void SetUVOffset(const Vector2& uvOffset) { m_uvOffset = uvOffset; m_isProxyDirty = true; }
	void SetPivot(const Vector2& pivot) { m_pivot = pivot; m_isProxyDirty = true; }
	void SetBillboardType(BillboardType type) { m_billboardType = type; m_isProxyDirty = true; }
	void SetFlipX(bool flip) { m_flipX = flip; m_isProxyDirty = true; }
	void SetFlipY(bool flip) { m_flipY = flip; m_isProxyDirty = true; }

	Vector4 GetColor() const { return m_color; }
	Vector2 GetUVScale() const { return m_uvScale; }
	Vector2 GetUVOffset() const { return m_uvOffset; }
	Vector2 GetPivot() const { return m_pivot; }
	BillboardType GetBillboardType() const { return m_billboardType; }
	bool IsFlipX() const { return m_flipX; }
	bool IsFlipY() const { return m_flipY; }

	const SpriteRenderTemplate& GetRenderTemplate() const { return m_template; }
	const SpriteRendererProxy& GetRenderProxy();

	void SetVisibility(bool visible) { m_isVisible = visible; m_isProxyDirty = true; }
	bool IsVisible() const { return m_isVisible; }
	bool IsConfigured() const { return m_isConfigured; }

private:
	Vector4 m_color{ 1,1,1,1 };								// Color for rendering (can be used to tint the mesh)
	Vector2 m_uvScale{ 1,1 };								// UV scale for texture mapping
	Vector2 m_uvOffset{ 0,0 };								// UV offset for texture mapping
	Vector2 m_pivot{ 0.5f, 0.5f };							// Pivot point for the sprite
	BillboardType m_billboardType = BillboardType::None;	// Type of billboard to use for this sprite
	bool m_flipX = false;									// Whether to flip the sprite horizontally
	bool m_flipY = false;									// Whether to flip the sprite vertically

	SpriteRenderTemplate m_template;	// Render template for this sprite (contains the texture and billboard type)
	SpriteRendererProxy m_proxy;		// Cached render proxy for this component (used for rendering)
	bool m_isProxyDirty = true;			// Flag to indicate if the render proxy needs to be updated
	uint64_t m_transformGeneration = 0;	// Generation counter for the transform

	bool m_isVisible = true;		// Visibility flag for the mesh
	bool m_isConfigured = false;	// Flag to indicate if the render templates have been configured (used to prevent rendering before initialization)

private:
	void RebuildRenderProxy();		// Rebuild the render proxy (Called when GetRenderProxy is called and the transform is dirty)
	void CheckIfTransformChanged();	// Check if the transform has changed since the last frame and mark the render proxy as dirty if it has
};
