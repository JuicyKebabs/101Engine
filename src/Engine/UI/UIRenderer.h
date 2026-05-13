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
	Vector2 pivot = { 0.5f, 0.5f };		// Pivot point for the UI element (normalized, where (0,0) is top-left and (1,1) is bottom-right)
	Vector2 flip = { 1, 1 };			// Flip flags for X and Y axes (1 for normal, -1 for flipped)
};

class UIRenderer : public RendererComponent
{
public:
	struct InitDesc : public RendererComponent::InitDesc
	{
		Canvas* pCanvas = nullptr;		// Pointer to the canvas this UI element belongs to (used for sorting and rendering)
		UIRenderTemplate renderTemplate;
		UINT order = 0;
		Vector4 color{ 1,1,1,1 };
		Vector2 uvScale{ 1,1 };
		Vector2 uvOffset{ 0,0 };
		Vector2 pivot{ 0.5f, 0.5f };
		bool flipX = false;
		bool flipY = false;
		InitDesc(const std::string& name = "UIRenderer") : RendererComponent::InitDesc(Vector4(1, 1, 1, 1), true, name) {}
		InitDesc(Canvas* pCanvas, const UIRenderTemplate& renderTemplate, UINT order, const Vector4& color, const Vector2& uvScale, const Vector2& uvOffset, const Vector2& pivot, bool flipX, bool flipY, const std::string& name = "UIRenderer")
			: RendererComponent::InitDesc(color, true, name), pCanvas(pCanvas), renderTemplate(renderTemplate), order(order), uvScale(uvScale), uvOffset(uvOffset), pivot(pivot), flipX(flipX), flipY(flipY) {}
	};

public:
	UIRenderer() = default;
	~UIRenderer() = default;
	void Init(const InitDesc& desc) { 
		assert(desc.pCanvas && "UIRenderer requires a valid Canvas pointer");
		m_pCanvas = desc.pCanvas;
		m_renderTemplate = desc.renderTemplate;
		m_order = desc.order;
		m_uvScale = desc.uvScale;
		m_uvOffset = desc.uvOffset;
		m_pivot = desc.pivot;
		m_flipX = desc.flipX;
		m_flipY = desc.flipY;
		RendererComponent::Init(desc); 
	}

	// Setters
	void SetCanvasOrder(UINT canvasOrder) { m_canvasOrder = canvasOrder; m_isProxyDirty = true; }
	void SetUVScale(const Vector2& uvScale) { m_uvScale = uvScale; m_isProxyDirty = true; }
	void SetUVOffset(const Vector2& uvOffset) { m_uvOffset = uvOffset; m_isProxyDirty = true; }
	void SetPivot(const Vector2& pivot) { m_pivot = pivot; m_isProxyDirty = true; }
	void SetFlipX(bool flip) { m_flipX = flip; m_isProxyDirty = true; }
	void SetFlipY(bool flip) { m_flipY = flip; m_isProxyDirty = true; }

	// Getters
	const UIRenderTemplate& GetRenderTemplate() const { return m_renderTemplate; }
	const UIRendererProxy& GetRenderProxy();
	UINT GetCanvasOrder() const { return m_pCanvas ? m_canvasOrder : 0; }
	UINT GetOrder() const { return m_order; }
	Vector2 GetUVScale() const { return m_uvScale; }
	Vector2 GetUVOffset() const { return m_uvOffset; }
	Vector2 GetPivot() const { return m_pivot; }
	bool IsFlipX() const { return m_flipX; }
	bool IsFlipY() const { return m_flipY; }
	bool IsVisible() const override;

	void OnCanvasDestroyed() { m_pCanvas = nullptr; }

private:
	UIRenderTemplate m_renderTemplate;	// Render template containing static rendering information for this UI element
	UIRendererProxy m_renderProxy;		// Render proxy containing dynamic rendering information for this UI element
	bool m_isProxyDirty = true;			// Flag to indicate if the render proxy needs to be updated
	uint64_t m_transformGeneration = 0;	// Generation counter for the transform

	Canvas* m_pCanvas = nullptr;	// Pointer to the canvas this UI element belongs to (used for sorting and rendering)
	UINT m_canvasOrder = 0;			// Order of the canvas this UI element belongs to (used for sorting within the canvas)
	UINT m_order = 0;				// Render order for this UI element (used for sorting)
	Vector2 m_uvScale{ 1,1 };		// UV scale for texture mapping
	Vector2 m_uvOffset{ 0,0 };		// UV offset for texture mapping
	Vector2 m_pivot{ 0.5f, 0.5f };	// Pivot point for the UI element (normalized, where (0,0) is top-left and (1,1) is bottom-right)
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