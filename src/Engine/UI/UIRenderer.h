#pragma once
#include "Engine/Component/RendererComponent.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Core/Debug/Debug.h"

class Canvas;

struct UIRendererProxy
{
	CommonRendererProxy common;			// Common render proxy data (position, world matrix, color, visibility)
	UINT canvasOrder = 0;				// Order of the canvas this UI element belongs to (used for sorting within the canvas)
	UINT order = 0;						// Render order for this UI element (used for sorting)
	Vector2 uvScale = { 1, 1 };			// UV scale for texture mapping
	Vector2 uvOffset = { 0, 0 };		// UV offset for texture mapping
	Vector2 flip = { 1, 1 };			// Flip flags for X and Y axes (1 for normal, -1 for flipped)
};

class UIRenderer : public RendererComponent
{
public:
	UIRenderer() = default;
	~UIRenderer() = default;

	// Setters
	void SetUVScale(const Vector2& uvScale) { m_uvScale = uvScale; m_isProxyDirty = true; }
	void SetUVOffset(const Vector2& uvOffset) { m_uvOffset = uvOffset; m_isProxyDirty = true; }
	void SetFlipX(bool flip) { m_flipX = flip; m_isProxyDirty = true; }
	void SetFlipY(bool flip) { m_flipY = flip; m_isProxyDirty = true; }

	// Getters
	const UIRenderTemplate& GetRenderTemplate() const { return m_renderTemplate; }
	const UIRendererProxy& GetRenderProxy();
	UINT GetOrder() const { return m_order; }
	Vector2 GetUVScale() const { return m_uvScale; }
	Vector2 GetUVOffset() const { return m_uvOffset; }
	bool IsFlipX() const { return m_flipX; }
	bool IsFlipY() const { return m_flipY; }
	bool IsVisible() const override;
	bool IsConfigured() const override { return !m_renderTemplate.empty(); }	// Check if the renderer has been configured with necessary resources (at least one render template)

	UINT GetCanvasOrder() const;
	void OnCanvasDestroyed() { m_pCanvas = nullptr; }

protected:
	UIRenderTemplate m_renderTemplate;	// Render template containing static rendering information for this UI element
	UIRendererProxy m_renderProxy;		// Render proxy containing dynamic rendering information for this UI element

	Canvas* m_pCanvas = nullptr;	// Pointer to the canvas this UI element belongs to (used for sorting and rendering)
	UINT m_order = 0;				// Render order for this UI element (used for sorting)
	Vector2 m_uvScale{ 1,1 };		// UV scale for texture mapping
	Vector2 m_uvOffset{ 0,0 };		// UV offset for texture mapping
	bool m_flipX = false;			// Flip flag for X axis (false for normal, true for flipped)
	bool m_flipY = false;			// Flip flag for Y axis (false for normal, true for flipped)

private:
	// Override functions for component lifecycle
	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDestroyOverride() override;

	void RebuildRenderProxy();		// Rebuild the render proxy based on the current state of the component (e.g., transform, color, UV settings)
	void InitialRegistration();		// Register this UI renderer with the canvas for sorting and rendering
};