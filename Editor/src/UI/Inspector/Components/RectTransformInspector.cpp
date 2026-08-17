#include "UI/Inspector/Components/RectTransformInspector.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Actor/Actor.h"
#include "Engine/UI/Canvas.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void RectTransformInspector::Draw(RectTransform& rectTransform, const InspectorContext&)
{
	int anchorMode = static_cast<int>(rectTransform.GetAnchorMode());
	Vector2 anchoredPosition = rectTransform.GetAnchoredPosition();
	Vector3 rotation = rectTransform.GetLocalRotationEulerDeg();
	Vector3 scale = rectTransform.GetLocalScale();
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

	if (EditorUI::Vector2Field("Position", anchoredPosition))
	{
		rectTransform.SetAnchoredPosition(anchoredPosition);
	}

	if (EditorUI::Vector3Field("Rotation", rotation, 0.5f))
	{
		rectTransform.SetLocalRotationEulerDeg(rotation);
	}

	if (EditorUI::Vector3Field("Scale", scale, 0.1f))
	{
		rectTransform.SetLocalScale(scale);
	}

	if (EditorUI::Vector2Field("Pivot", pivot))
	{
		rectTransform.SetPivot(pivot);
	}

	Actor* owner = rectTransform.GetOwner();
	Canvas* canvas = owner
		? owner->GetComponentByClass<Canvas>()
		: nullptr;
	const char* sizeLabel = canvas && !canvas->IsRootCanvas()
		? "Display Size"
		: "Size";

	if (EditorUI::Vector2Field(sizeLabel, size))
	{
		rectTransform.SetSizeDelta(size);
	}

	EditorUI::EndPropertyGrid();
}
