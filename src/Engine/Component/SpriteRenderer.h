#pragma once
#include "Engine/Component/RendererComponent.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Component/Camera.h"

struct SpriteRendererProxy
{
	CommonRendererProxy common;		// Common render proxy data (position, world matrix, color, visibility)
	Vector2 uvScale{ 1,1 };			// UV scale for texture mapping
	Vector2 uvOffset{ 0,0 };		// UV offset for texture mapping
	Vector2 pivot{ 0.5f, 0.5f };	// Pivot point for the sprite
	Vector2 flip{ 1,1 };			// Flip flags for X and Y axes (1 for normal, -1 for flipped)
};

class SpriteRenderer : public RendererComponent
{
public:
	struct ParamDesc
	{
		SpriteRenderTemplate renderTemplate;				// Render template for this sprite
		Vector2 uvScale{ 1,1 };								// UV scale for texture mapping
		Vector2 uvOffset{ 0,0 };							// UV offset for texture mapping
		Vector2 pivot{ 0.5f, 0.5f };						// Pivot point for the sprite
		BillboardType billboardType = BillboardType::None;	// Type of billboard to use for this sprite
		bool flipX = false;									// Whether to flip the sprite horizontally
		bool flipY = false;									// Whether to flip the sprite vertically
		Vector4 color{ 1,1,1,1 };							// Color for rendering (can be used to tint the rendered object)
		bool visible = true;								// Visibility flag for this renderer
		std::string name = "SpriteRenderer";				// Component name (optional, can be used for debugging or identification)
	};

public:
	SpriteRenderer() = default;
	~SpriteRenderer() = default;
	void SetParams(const ParamDesc& desc) {
		m_template = desc.renderTemplate;
		m_isProxyDirty = true;
		SetUVScale(desc.uvScale);
		SetUVOffset(desc.uvOffset);
		SetPivot(desc.pivot);
		SetBillboardType(desc.billboardType);
		SetFlipX(desc.flipX);
		SetFlipY(desc.flipY);
		SetColor(desc.color);
		SetVisible(desc.visible);
		SetName(desc.name);
	}

	// Setters
	void SetUVScale(const Vector2& uvScale) { m_uvScale = uvScale; m_isProxyDirty = true; }
	void SetUVOffset(const Vector2& uvOffset) { m_uvOffset = uvOffset; m_isProxyDirty = true; }
	void SetPivot(const Vector2& pivot) { m_pivot = pivot; m_isProxyDirty = true; }
	void SetBillboardType(BillboardType type) { m_billboardType = type; m_isProxyDirty = true; }
	void SetFlipX(bool flip) { m_flipX = flip; m_isProxyDirty = true; }
	void SetFlipY(bool flip) { m_flipY = flip; m_isProxyDirty = true; }

	// Getters
	const SpriteRenderTemplate& GetRenderTemplate() const { return m_template; }
	const SpriteRendererProxy& GetRenderProxy(const CameraInfo& cameraInfo);
	Vector2 GetUVScale() const { return m_uvScale; }
	Vector2 GetUVOffset() const { return m_uvOffset; }
	Vector2 GetPivot() const { return m_pivot; }
	BillboardType GetBillboardType() const { return m_billboardType; }
	bool IsFlipX() const { return m_flipX; }
	bool IsFlipY() const { return m_flipY; }
	bool IsConfigured() const override { return m_template.materialDesc.textureHandle != InvalidTextureHandle; }	// Check if the render template is valid (has a texture)

private:
	Vector2 m_uvScale{ 1,1 };								// UV scale for texture mapping
	Vector2 m_uvOffset{ 0,0 };								// UV offset for texture mapping
	Vector2 m_pivot{ 0.5f, 0.5f };							// Pivot point for the sprite
	BillboardType m_billboardType = BillboardType::None;	// Type of billboard to use for this sprite
	bool m_flipX = false;									// Whether to flip the sprite horizontally
	bool m_flipY = false;									// Whether to flip the sprite vertically

	SpriteRenderTemplate m_template;	// Render template for this sprite (contains the texture and billboard type)
	SpriteRendererProxy m_proxy;		// Cached render proxy for this component (used for rendering)

private:
	// Override functions for component lifecycle
	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDestroyOverride() override;

	void RebuildRenderProxy(const CameraInfo& cameraInfo);	// Rebuild the render proxy (Called when GetRenderProxy is called and the transform is dirty)
};
