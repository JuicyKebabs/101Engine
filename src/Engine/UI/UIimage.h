#pragma once
#include "UIRenderer.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Core/Math/Math.h"

class UIImage : public UIRenderer
{
public:
	struct ParamDesc
	{
		Canvas* pCanvas = nullptr;			// Pointer to the canvas this UI element belongs to (used for sorting and rendering)
		UIRenderTemplate renderTemplate;	// Render template containing static rendering information for this UI element
		UINT order = 0;						// Render order for this UI element (used for sorting)
		Vector4 color{ 1,1,1,1 };			// Color of the UI element
		Vector2 uvScale{ 1,1 };				// UV scale for texture mapping
		Vector2 uvOffset{ 0,0 };			// UV offset for texture mapping
		bool flipX = false;					// Flip flag for X axis (false for normal, true for flipped)
		bool flipY = false;					// Flip flag for Y axis (false for normal, true for flipped)
		std::string name = "UIImage";		// Component name (optional, can be used for debugging or identification)
	};

public:
	UIImage() = default;
	~UIImage() = default;
	void SetParams(const ParamDesc& desc) {
		m_pCanvas = desc.pCanvas;
		m_renderTemplate = desc.renderTemplate;
		m_order = desc.order;
		m_color = desc.color;
		m_uvScale = desc.uvScale;
		m_uvOffset = desc.uvOffset;
		m_flipX = desc.flipX;
		m_flipY = desc.flipY;
		SetName(desc.name);
	}
};
