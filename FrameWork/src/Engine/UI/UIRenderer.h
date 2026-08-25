#pragma once
#include <optional>
#include "Engine/Component/RendererComponent.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/GUID/Guid.h"

class Canvas;

struct UIRendererProxy
{
	CommonRendererProxy common;			// Common render proxy data (position, world matrix, color, visibility)
	Vector2 uvScale = { 1, 1 };			// UV scale for texture mapping
	Vector2 uvOffset = { 0, 0 };		// UV offset for texture mapping
	Vector2 flip = { 1, 1 };			// Flip flags for X and Y axes (1 for normal, -1 for flipped)
};

class UIRenderer : public RendererComponent
{
public:
	UIRenderer() = default;
	~UIRenderer() = default;

	void InvalidateRenderProxy() { m_isProxyDirty = true; }	// Mark the render proxy as dirty

	// Setters
	void SetUVScale(const Vector2& uvScale) { m_uvScale = uvScale; m_isProxyDirty = true; }
	void SetUVOffset(const Vector2& uvOffset) { m_uvOffset = uvOffset; m_isProxyDirty = true; }
	void SetFlipX(bool flip) { m_flipX = flip; m_isProxyDirty = true; }
	void SetFlipY(bool flip) { m_flipY = flip; m_isProxyDirty = true; }

	// Getters
	const UIRenderTemplate& GetRenderTemplate() const { return m_renderTemplate; }
	const UIRendererProxy& GetRenderProxy();
	UINT GetOrder() const { return GetSortOrderInCanvas(); }
	Vector2 GetUVScale() const { return m_uvScale; }
	Vector2 GetUVOffset() const { return m_uvOffset; }
	bool IsFlipX() const { return m_flipX; }
	bool IsFlipY() const { return m_flipY; }
	bool IsVisible() const override;
	bool IsConfigured() const override { return !m_renderTemplate.empty(); }	// Check if the renderer has been configured with necessary resources (at least one render template)

	void SetGoverningCanvas(Canvas* canvas) override;
	void SetCanvas(Canvas* canvas) { SetGoverningCanvas(canvas); }
	void SetOrder(UINT order) { SetSortOrderInCanvas(order); m_isProxyDirty = true; }

	Canvas* GetCanvas() const { return GetGoverningCanvas(); }
	void OnCanvasDestroyed() { SetGoverningCanvas(nullptr); }

	// Serialization and deserialization methods
	bool Serialize(nlohmann::json& outJson) const override;
	bool Deserialize(const nlohmann::json& json) override;
	bool ResolveReferences(SceneBase& scene) override;

protected:
	UIRenderTemplate m_renderTemplate;	// Render template containing static rendering information for this UI element
	UIRendererProxy m_renderProxy;		// Render proxy containing dynamic rendering information for this UI element

	Vector2 m_uvScale{ 1,1 };		// UV scale for texture mapping
	Vector2 m_uvOffset{ 0,0 };		// UV offset for texture mapping
	bool m_flipX = false;			// Flip flag for X axis (false for normal, true for flipped)
	bool m_flipY = false;			// Flip flag for Y axis (false for normal, true for flipped)

	std::optional<Guid> m_pendingCanvasActorId;	// Optional Guid of the canvas actor to which this UI element should be registered (used for deferred registration if the canvas is not yet available)

private:
	// Override functions for component lifecycle
	void OnAttachOverride() override;
	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDetachOverride() override;
	void OnDestroyOverride() override;

	void RebuildRenderProxy();		// Rebuild the render proxy based on the current state of the component (e.g., transform, color, UV settings)
	void InitialRegistration();		// Register this UI renderer with the canvas for sorting and rendering
};
