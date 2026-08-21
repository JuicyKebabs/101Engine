#include "UI/Inspector/Components/RectTransformInspector.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Actor/Actor.h"
#include "Engine/UI/Canvas.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void RectTransformInspector::Draw(RectTransform& rectTransform, const InspectorContext& context)
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

	// Capture the current state of the RectTransform before editing
	RectTransformEditState beforeState = RectTransformEditState::Capture(rectTransform);

	bool anyActivated = false;				// Indicates if any user started editing a field (e.g., grabbed a bar or clicked on a field)
	bool anyDeactivatedAfterEdit = false;	// Indicates if any user finished editing a field (e.g., released a bar or clicked away from a field)

	if (!EditorUI::BeginPropertyGrid("RectTransformProperties")) return;

	EditorUI::EditResult result{};

	// Anchor Mode
	if (EditorUI::ComboField("Anchor", anchorMode, anchorItems, 9))
	{
		rectTransform.SetAnchorMode(static_cast<AnchorMode>(anchorMode));
		anyActivated = true;
		anyDeactivatedAfterEdit = true;
	}

	// Position
	result = EditorUI::Vector2Field("Position", anchoredPosition);

	if (result.changed) rectTransform.SetAnchoredPosition(anchoredPosition);
	anyActivated |= result.activated;
	anyDeactivatedAfterEdit |= result.deactivatedAfterEdit;

	// Rotation
	result = EditorUI::Vector3Field("Rotation", rotation, 0.5f);

	if (result.changed) rectTransform.SetLocalRotationEulerDeg(rotation);
	anyActivated |= result.activated;
	anyDeactivatedAfterEdit |= result.deactivatedAfterEdit;

	// Scale
	result = EditorUI::Vector3Field("Scale", scale, 0.1f);

	if (result.changed) rectTransform.SetLocalScale(scale);
	anyActivated |= result.activated;
	anyDeactivatedAfterEdit |= result.deactivatedAfterEdit;

	// Pivot
	result = EditorUI::Vector2Field("Pivot", pivot);

	if (result.changed) rectTransform.SetPivot(pivot);
	anyActivated |= result.activated;
	anyDeactivatedAfterEdit |= result.deactivatedAfterEdit;

	// Size
	Actor* owner = rectTransform.GetOwner();
	Canvas* canvas = owner
		? owner->GetComponentByClass<Canvas>() : nullptr;
	const char* sizeLabel = canvas && !canvas->IsRootCanvas()
		? "Display Size" : "Size";

	result = EditorUI::Vector2Field(sizeLabel, size);

	if (result.changed) rectTransform.SetSizeDelta(size);
	anyActivated |= result.activated;
	anyDeactivatedAfterEdit |= result.deactivatedAfterEdit;

	// Capture the state of the RectTransform after editing
	RectTransformEditState afterState = RectTransformEditState::Capture(rectTransform);

	if (anyActivated && context.onRectTransformEditBegin && owner)
	{
		context.onRectTransformEditBegin(owner->GetGuid(), beforeState);
	}

	if (anyDeactivatedAfterEdit && context.onRectTransformEditEnd && owner)
	{
		context.onRectTransformEditEnd(owner->GetGuid(), afterState);
	}

	EditorUI::EndPropertyGrid();
}