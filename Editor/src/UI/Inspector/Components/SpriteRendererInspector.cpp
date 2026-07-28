#include "UI/Inspector/Components/SpriteRendererInspector.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Resource/AssetManager.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void SpriteRendererInspector::Draw(SpriteRenderer& spriteRenderer, const InspectorContext& context)
{
	Guid selectedTextureId = spriteRenderer.GetTextureAssetId();
	bool visible = spriteRenderer.IsVisible();
	Vector4 color = spriteRenderer.GetColor();
	int billboardType = static_cast<int>(spriteRenderer.GetBillboardType());
	Vector2 uvScale = spriteRenderer.GetUVScale();
	Vector2 uvOffset = spriteRenderer.GetUVOffset();
	Vector2 pivot = spriteRenderer.GetPivot();
	bool flipX = spriteRenderer.IsFlipX();
	bool flipY = spriteRenderer.IsFlipY();

	static const char* billboardItems[] = {
		"None",
		"Spherical",
		"Cylindrical"
	};

	if (!EditorUI::BeginPropertyGrid("SpriteRendererProperties")) return;

	if (context.assetManager)
	{
		const Guid currentTextureId = spriteRenderer.GetTextureAssetId();

		if (EditorUI::AssetField("Texture", *context.assetManager, AssetType::Texture, currentTextureId, selectedTextureId))
		{
			spriteRenderer.SetTextureAsset(selectedTextureId);
		}
	}

	if (EditorUI::BoolField("Visible", visible)) spriteRenderer.SetVisible(visible);
	if (EditorUI::ColorField("Color", color)) spriteRenderer.SetColor(color);

	if (EditorUI::ComboField("Billboard", billboardType, billboardItems, 3))
	{
		spriteRenderer.SetBillboardType(static_cast<BillboardType>(billboardType));
	}

	if (EditorUI::Vector2Field("UV Scale", uvScale)) spriteRenderer.SetUVScale(uvScale);
	if (EditorUI::Vector2Field("UV Offset", uvOffset)) spriteRenderer.SetUVOffset(uvOffset);
	if (EditorUI::Vector2Field("Pivot", pivot)) spriteRenderer.SetPivot(pivot);
	if (EditorUI::BoolField("Flip X", flipX)) spriteRenderer.SetFlipX(flipX);
	if (EditorUI::BoolField("Flip Y", flipY)) spriteRenderer.SetFlipY(flipY);

	EditorUI::EndPropertyGrid();
}
