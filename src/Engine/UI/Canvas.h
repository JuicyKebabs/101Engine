#pragma once
#include <vector>
#include <utility>
#include "UIRenderer.h"

class Canvas : public Component
{
public:
	enum class RenderMode
	{
		ScreenSpace_Overlay,
		ScreenSpace_Camera,
		WorldSpace
	};

	struct InitDesc : Component::InitDesc {
		RenderMode renderMode = RenderMode::ScreenSpace_Overlay;
		UINT sortOrder = 0;
		bool isVisible = true;
		InitDesc(RenderMode renderMode = RenderMode::ScreenSpace_Overlay, UINT sortOrder = 0, bool isVisible = true, const std::string& name = "Canvas") 
			: Component::InitDesc(name), renderMode(renderMode), sortOrder(sortOrder), isVisible(isVisible) {}
	};

public:
	Canvas() = default;
	~Canvas() = default;
	void Init(const InitDesc& desc = InitDesc()) {
		m_renderMode = desc.renderMode;
		m_sortOrder = desc.sortOrder;
		m_isVisible = desc.isVisible;
		Component::Init(desc);
	}

	// Setters
	void SetVisible(bool flag) { m_isVisible = flag; }
	void SetSortOrder(UINT order) { m_sortOrder = order; }

	//Getters
	bool IsVisible() const { return m_isVisible; }
	UINT GetSortOrder() const { return m_sortOrder; }

	// UIRenderer management
	void RegisterUIRenderer(UIRenderer* ui) {
		if (ui) {
			ui->SetCanvasOrder(m_sortOrder); // Set the canvas order for sorting within the canvas
			m_uiList.push_back(ui);
		}
	}
	void UnregisterUIRenderer(UIRenderer* ui) {
		m_uiList.erase(std::remove(m_uiList.begin(), m_uiList.end(), ui), m_uiList.end());
	}

private:
	std::vector<UIRenderer*> m_uiList;
	RenderMode m_renderMode = RenderMode::ScreenSpace_Overlay;
	UINT m_sortOrder = 0;
	bool m_isVisible = true;

private:
	// Overrides
	void OnStartOverride() override {};
	void PreUpdateOverride(float deltaTime) override {};
	void UpdateOverride(float deltaTime) override {};
	void LateUpdateOverride(float deltaTime) override {};
	void OnDestroyOverride() override {
		for(auto* ui : m_uiList) if(ui) ui->OnCanvasDestroyed();
	};
};