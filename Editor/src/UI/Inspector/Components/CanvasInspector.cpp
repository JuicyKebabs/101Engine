#include "UI/Inspector/Components/CanvasInspector.h"
#include "Engine/UI/Canvas.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void CanvasInspector::Draw(Canvas& canvas, const InspectorContext&)
{
	bool visible = canvas.IsVisible();
	UINT sortOrder = canvas.GetSortOrder();

	if (!EditorUI::BeginPropertyGrid("CanvasProperties")) return;

	if (EditorUI::BoolField("Visible", visible)) canvas.SetVisible(visible);
	if (EditorUI::UIntField("Sort Order", sortOrder)) canvas.SetSortOrder(sortOrder);

	EditorUI::EndPropertyGrid();
}
