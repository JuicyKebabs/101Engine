#include "UI/Inspector/Components/UIRendererInspector.h"
#include "Engine/UI/UIRenderer.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void UIRendererInspector::Draw(UIRenderer& uiRenderer, const InspectorContext&)
{
	bool visible = uiRenderer.RendererComponent::IsVisible();
	Vector4 color = uiRenderer.GetColor();
	UINT order = uiRenderer.GetOrder();
	Vector2 uvScale = uiRenderer.GetUVScale();
	Vector2 uvOffset = uiRenderer.GetUVOffset();
	bool flipX = uiRenderer.IsFlipX();
	bool flipY = uiRenderer.IsFlipY();

	if (!EditorUI::BeginPropertyGrid("UIRendererProperties")) return;

	if (EditorUI::BoolField("Visible", visible)) uiRenderer.SetVisible(visible);
	if (EditorUI::ColorField("Color", color)) uiRenderer.SetColor(color);
	if (EditorUI::UIntField("Order", order)) uiRenderer.SetOrder(order);
	if (EditorUI::Vector2Field("UV Scale", uvScale)) uiRenderer.SetUVScale(uvScale);
	if (EditorUI::Vector2Field("UV Offset", uvOffset)) uiRenderer.SetUVOffset(uvOffset);
	if (EditorUI::BoolField("Flip X", flipX)) uiRenderer.SetFlipX(flipX);
	if (EditorUI::BoolField("Flip Y", flipY)) uiRenderer.SetFlipY(flipY);

	EditorUI::EndPropertyGrid();
}
