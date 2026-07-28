#include "UI/Inspector/Components/RectTransformInspector.h"
#include "Engine/Component/RectTransform.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void RectTransformInspector::Draw(RectTransform& rectTransform, const InspectorContext&)
{
	int anchorMode = static_cast<int>(rectTransform.GetAnchorMode());
	Vector2 anchoredPosition = rectTransform.GetAnchoredPosition();
	Vector2 pivot = rectTransform.GetPivot();
	Vector2 size = rectTransform.GetSize();

	static const char* anchorItems[] = {
		"Top Left",
		"Top Center",
		"Top Right",
		"Middle Left",
		"Middle Center",
		"Middle Right",
		"Bottom Left",
		"Bottom Center",
		"Bottom Right"
	};

	if (!EditorUI::BeginPropertyGrid("RectTransformProperties")) return;

	if (EditorUI::ComboField("Anchor", anchorMode, anchorItems, 9))
	{
		rectTransform.SetAnchorMode(static_cast<AnchorMode>(anchorMode));
	}

	if (EditorUI::Vector2Field("Position", anchoredPosition)) rectTransform.SetAnchoredPosition(anchoredPosition);
	if (EditorUI::Vector2Field("Pivot", pivot)) rectTransform.SetPivot(pivot);
	if (EditorUI::Vector2Field("Size", size)) rectTransform.SetSizeDelta(size);

	EditorUI::EndPropertyGrid();
}
