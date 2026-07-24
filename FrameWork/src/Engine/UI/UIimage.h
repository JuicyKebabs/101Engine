#pragma once
#include <optional>
#include "UIRenderer.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/GUID/Guid.h"

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
		bool isVisible = true;				// Visibility flag for this UI element
		std::string name = "UIImage";		// Component name (optional, can be used for debugging or identification)
	};

public:
	UIImage() = default;
	~UIImage() = default;
	void SetParams(const ParamDesc& desc) {
		SetCanvas(desc.pCanvas);
		m_renderTemplate = desc.renderTemplate;
		m_textureAssetId = {};
		m_pendingTextureAssetId.reset();
		SetOrder(desc.order);
		SetColor(desc.color);
		SetVisible(desc.isVisible);
		SetUVScale(desc.uvScale);
		SetUVOffset(desc.uvOffset);
		SetFlipX(desc.flipX);
		SetFlipY(desc.flipY);
		SetName(desc.name);
		m_isProxyDirty = true;
	}

	// Setters and getters for texture asset
	bool SetTextureAsset(const Guid& assetId);
	const Guid& GetTextureAssetId() const { return m_textureAssetId; }

	// Serialization and deserialization methods
	bool Serialize(nlohmann::json& outJson) const override;
	bool Deserialize(const nlohmann::json& json) override;
	bool ResolveReferences(SceneBase& scene) override;

private:
	Guid m_textureAssetId;							// Guid of the texture asset
	std::optional<Guid> m_pendingTextureAssetId;	// Optional Guid of the texture asset to be loaded
};
