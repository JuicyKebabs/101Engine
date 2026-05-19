#pragma once
#include <vector>
#include <utility>
#include "UIRenderer.h"

class Canvas : public Component
{
public:
	struct ParamDesc {
		UINT sortOrder = 0;
		bool isVisible = true;
		std::string name = "Canvas";
	};

public:
	Canvas() = default;
	~Canvas() = default;
	void Init(const ParamDesc& desc = ParamDesc()) {
		m_sortOrder = desc.sortOrder;
		m_isVisible = desc.isVisible;
		SetName(desc.name);
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
			m_uiList.push_back(ui);
		}
	}
	void UnregisterUIRenderer(UIRenderer* ui) {
		m_uiList.erase(std::remove(m_uiList.begin(), m_uiList.end(), ui), m_uiList.end());
	}

private:
	std::vector<UIRenderer*> m_uiList;
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