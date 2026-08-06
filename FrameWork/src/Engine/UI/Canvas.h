#pragma once
#include <vector>
#include <utility>
#include "UIRenderer.h"

//----------------------------------------------------------------
// Canvas class
// A component that manages a collection of UIRenderer components
//----------------------------------------------------------------

class SceneBase;

// Enumration of mode for rendering space of UI elements in a Canvas
enum class CanvasRenderMode
{
	ScreenSpace,	// Render UI elements in screen space
	WorldSpace,		// Render UI elements in world space
	Max
};

// Enumeration of scaling modes for Screen-Space UI elements in a Canvas
enum class CanvasScaleMode
{
	ConstantPixelSize,		// UI elements maintain a constant pixel size regardless of screen resolution
	ScaleWithScreenSize,	// UI elements scale with the screen size, maintaining relative proportions
	Max
};

class Canvas : public Component
{
public:
	struct ParamDesc 
	{
		CanvasRenderMode renderMode = CanvasRenderMode::ScreenSpace;
		CanvasScaleMode scaleMode = CanvasScaleMode::ScaleWithScreenSize;
		UINT sortOrder = 0;
		bool isVisible = true;
		Vector2 referenceSize{ 1920, 1080 };
		float matchWidthOrHeight = 0.5f;
		std::string name = "Canvas";
	};

public:
	Canvas() = default;
	~Canvas() = default;
	void SetParams(const ParamDesc& desc = ParamDesc()) 
	{
		m_authoredRenderMode = desc.renderMode;
		m_scaleMode = desc.scaleMode;
		m_effectiveRenderMode = desc.renderMode;
		m_sortOrder = desc.sortOrder;
		m_isVisible = desc.isVisible;
		m_referenceSize = desc.referenceSize;
		m_matchWidthOrHeight = desc.matchWidthOrHeight;
		SetName(desc.name);
	}

	// Setters
	void SetVisible(bool flag) { m_isVisible = flag; }
	void SetSortOrder(UINT order) 
	{ 
		if (m_sortOrder == order) return;
		m_sortOrder = order; 
		InvalidateAllUIRendererProxies(); 
	}

	//Getters
	// RenderMode authored directly on this Canvas
	CanvasRenderMode GetAuthoredRenderMode() const { return m_authoredRenderMode; }

	// RenderMode actually used after resolving Canvas hierarchy inheritance
	CanvasRenderMode GetRenderMode() const { return m_effectiveRenderMode; }

	// Get the reference size used for layout calculations 
	// based on the render mode and hierarcy
	Vector2 GetLayoutReferenceSize() const;

	// Get the scale factor for Screen-Space UI elements in this Canvas 
	// based on the scaling mode and reference size
	float GetScaleFactor() const;

	// Chack if this Canvas is a root canvas (no ancestor Canvas in the hierarchy)
	bool IsRootCanvas() const;

	CanvasScaleMode GetScaleMode() const { return m_scaleMode; }
	Vector2 GetReferenceSize() const { return m_referenceSize; }
	float GetMatchWidthOrHeight() const { return m_matchWidthOrHeight; }
	bool IsVisible() const { return m_isVisible; }
	UINT GetSortOrder() const { return m_sortOrder; }

	// UIRenderer management
	void RegisterUIRenderer(UIRenderer* ui) 
	{
		if (ui) {
			m_uiList.push_back(ui);
		}
	}
	void UnregisterUIRenderer(UIRenderer* ui) {

		m_uiList.erase(std::remove(m_uiList.begin(), m_uiList.end(), ui), m_uiList.end());
	}

	// Serialization and deserialization methods
	bool Serialize(nlohmann::json& outJson) const override;
	bool Deserialize(const nlohmann::json& json) override;

private:
	std::vector<UIRenderer*> m_uiList;

	CanvasRenderMode m_authoredRenderMode = CanvasRenderMode::ScreenSpace;	// Set by the user
	CanvasRenderMode m_effectiveRenderMode = CanvasRenderMode::ScreenSpace;	// Effective mode based on hierarchy constraints

	// Scaling mode for Screen-Space UI elements in this Canvas
	CanvasScaleMode m_scaleMode = CanvasScaleMode::ScaleWithScreenSize;

	// Reference size for layout calculations in this Canvas
	Vector2 m_referenceSize{ 1920.0f, 1080.0f };


	float m_matchWidthOrHeight = 0.5f;
	
	UINT m_sortOrder = 0;
	bool m_isVisible = true;

	

private:
	// Overrides
	void OnStartOverride() override {};
	void PreUpdateOverride(float deltaTime) override {};
	void UpdateOverride(float deltaTime) override {};
	void LateUpdateOverride(float deltaTime) override {};
	void OnDestroyOverride() override 
	{
		auto registeredUI = std::move(m_uiList);
		m_uiList.clear();

		for (auto* ui : registeredUI)
		{
			if (ui) ui->OnCanvasDestroyed();
		}
	};

	void InvalidateAllUIRendererProxies()
	{
		for (auto* ui : m_uiList)
		{
			if (ui) ui->InvalidateRenderProxy();
		}
	}

private:

	// Set the authored mode of a topmost Canvas.
	// A topmost Canvas uses its authored value as its effective value.
	void SetAuthoredRenderMode(CanvasRenderMode mode)
	{
		const bool authoredChanged = m_authoredRenderMode != mode;
		const bool effectiveChanged = m_effectiveRenderMode != mode;

		if (!authoredChanged && !effectiveChanged) return;

		m_authoredRenderMode = mode;
		m_effectiveRenderMode = mode;

		InvalidateAllUIRendererProxies();
	}

	// Apply a mode inherited from a governing Canvas.
	// This must not overwrite the Canvas's authored setting.
	void SetInheritedRenderMode(CanvasRenderMode mode)
	{
		if (m_effectiveRenderMode == mode) return;

		m_effectiveRenderMode = mode;
		InvalidateAllUIRendererProxies();
	}

	// Use this Canvas's own authored setting after it is detached
	// from an ancestor Canvas hierarchy.
	void RestoreAuthoredRenderMode()
	{
		if (m_effectiveRenderMode == m_authoredRenderMode) return;

		m_effectiveRenderMode = m_authoredRenderMode;
		InvalidateAllUIRendererProxies();
	}

	// Set the reference size for layout calculations in this Canvas.
	void SetReferenceSize(const Vector2& size)
	{
		if (m_referenceSize == size) return;

		m_referenceSize = size;
		InvalidateAllUIRendererProxies();
	}

	// Set the scaling mode for Screen-Space UI elements in this Canvas.
	void SetScaleMode(CanvasScaleMode mode)
	{
		if (m_scaleMode == mode) return;

		m_scaleMode = mode;
		InvalidateAllUIRendererProxies();
	}

	// Set the match width or height factor for 
	// scaling Screen-Space UI elements in this Canvas.
	void SetMatchWidthOrHeight(float match)
	{
		if (m_matchWidthOrHeight == match) return;

		m_matchWidthOrHeight = match;
		InvalidateAllUIRendererProxies();
	}

	friend class SceneBase;
};
