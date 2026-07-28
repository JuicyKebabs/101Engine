#include "UI/Inspector/Components/UIImageInspector.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/UI/UIimage.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void UIImageInspector::Draw(UIImage& image, const InspectorContext& context)
{
	Guid selectedTextureId = image.GetTextureAssetId();
	bool visible = image.RendererComponent::IsVisible();
	Vector4 color = image.GetColor();
	UINT order = image.GetOrder();
	Vector2 uvScale = image.GetUVScale();
	Vector2 uvOffset = image.GetUVOffset();
	bool flipX = image.IsFlipX();
	bool flipY = image.IsFlipY();

	if (!EditorUI::BeginPropertyGrid("UIImageProperties")) return;

	if (context.assetManager)
	{
		const Guid currentTextureId = image.GetTextureAssetId();

		if (EditorUI::AssetField("Texture", *context.assetManager, AssetType::Texture, currentTextureId, selectedTextureId))
		{
			image.SetTextureAsset(selectedTextureId);
		}
	}

	if (EditorUI::BoolField("Visible", visible)) image.SetVisible(visible);
	if (EditorUI::ColorField("Color", color)) image.SetColor(color);
	if (EditorUI::UIntField("Order", order)) image.SetOrder(order);
	if (EditorUI::Vector2Field("UV Scale", uvScale)) image.SetUVScale(uvScale);
	if (EditorUI::Vector2Field("UV Offset", uvOffset)) image.SetUVOffset(uvOffset);
	if (EditorUI::BoolField("Flip X", flipX)) image.SetFlipX(flipX);
	if (EditorUI::BoolField("Flip Y", flipY)) image.SetFlipY(flipY);

	EditorUI::EndPropertyGrid();
}
