#pragma once
#include <vector>
#include <utility>
#include "UIRenderer.h"

//----------------------------------------------------------------
// Canvas class
// A component that manages a collection of UIRenderer components
//----------------------------------------------------------------

class SceneBase;

enum class CanvasRenderMode
{
	ScreenSpace,	// Render UI elements in screen space
	WorldSpace,		// Render UI elements in world space
	Max				// Sentinel value for validation
};

class Canvas : public Component
{
public:
	struct ParamDesc 
	{
		CanvasRenderMode renderMode = CanvasRenderMode::ScreenSpace;
		UINT sortOrder = 0;
		bool isVisible = true;
		Vector2 referenceSize{ 1920, 1080 };
		std::string name = "Canvas";
	};

public:
	Canvas() = default;
	~Canvas() = default;
	void SetParams(const ParamDesc& desc = ParamDesc()) 
	{
		m_renderMode = desc.renderMode;
		m_sortOrder = desc.sortOrder;
		m_isVisible = desc.isVisible;
		m_worldReferenceSize = desc.referenceSize;
		SetName(desc.name);
	}

	// Return the reference size used for layout calculations based on the render mode and hierarchy
	Vector2 GetLayoutReferenceSize() const;

	// Setters
	void SetVisible(bool flag) { m_isVisible = flag; }
	void SetSortOrder(UINT order) 
	{ 
		if (m_sortOrder == order) return;
		m_sortOrder = order; 
		InvalidateAllUIRendererProxies(); 
	}

	//Getters
	CanvasRenderMode GetRenderMode() const { return m_renderMode; }
	bool IsVisible() const { return m_isVisible; }
	UINT GetSortOrder() const { return m_sortOrder; }
	Vector2 GetWorldReferenceSize() const { return m_worldReferenceSize; }

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
	CanvasRenderMode m_renderMode = CanvasRenderMode::ScreenSpace;
	UINT m_sortOrder = 0;
	bool m_isVisible = true;

	// Logical layout size used by direct RectTransform children of a root World-Space Canvas
	Vector2 m_worldReferenceSize{ 1920, 1080 };

private:
	// Overrides
	void OnStartOverride() override {};
	void PreUpdateOverride(float deltaTime) override {};
	void UpdateOverride(float deltaTime) override {};
	void LateUpdateOverride(float deltaTime) override {};
	void OnDestroyOverride() override 
	{
		for(auto* ui : m_uiList) if(ui) ui->OnCanvasDestroyed();
	};

	void InvalidateAllUIRendererProxies()
	{
		for (auto* ui : m_uiList)
		{
			if (ui) ui->InvalidateRenderProxy();
		}
	}

private:

	// Allow SceneBase to modify the render mode of the canvas
	void SetRenderMode(CanvasRenderMode mode) 
	{
		if (m_renderMode == mode) return;
		m_renderMode = mode; 
		InvalidateAllUIRendererProxies(); 
	}

	void SetWorldReferenceSize(const Vector2& size)
	{
		if (m_worldReferenceSize == size) return;
		m_worldReferenceSize = size;
		InvalidateAllUIRendererProxies();
	}

	friend class SceneBase;
};