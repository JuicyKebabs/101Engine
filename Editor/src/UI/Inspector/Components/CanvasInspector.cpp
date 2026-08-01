#include "UI/Inspector/Components/CanvasInspector.h"
#include "Engine/UI/Canvas.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

void CanvasInspector::Draw(Canvas& canvas, const InspectorContext&)
{
	bool visible = canvas.IsVisible();
	UINT sortOrder = canvas.GetSortOrder();

	if (!EditorUI::BeginPropertyGrid("CanvasProperties")) return;

	// Visibility
	if (EditorUI::BoolField("Visible", visible)) canvas.SetVisible(visible);

	// Render Mode
	CanvasRenderMode renderMode = canvas.GetRenderMode();
	int selectedMode = static_cast<int>(renderMode);
	const char* renderModes[] =
	{
		"Screen Space",
		"World Space"
	};

	if (EditorUI::ComboField("Render Mode", selectedMode, renderModes, static_cast<int>(CanvasRenderMode::Max)))
	{
		renderMode = static_cast<CanvasRenderMode>(selectedMode);

		// Set render mode through the scene to ensure all canvases in the hierarchy are synchronized
		Actor* owner = canvas.GetOwner();
		SceneBase* scene = owner ? owner->GetOwner() : nullptr;
		if (scene)
		{
			scene->SetCanvasRenderMode(&canvas, renderMode);
		}
	}

	// Sort Order
	if (EditorUI::UIntField("Sort Order", sortOrder)) canvas.SetSortOrder(sortOrder);
	
	// Reference Size (only for World Space canvases)
	if (canvas.GetRenderMode() == CanvasRenderMode::WorldSpace)
	{
		Vector2 referenceSize = canvas.GetWorldReferenceSize();

		if (EditorUI::Vector2Field("Reference Size", referenceSize, 1.0f))
		{
			Actor* owner = canvas.GetOwner();
			SceneBase* scene = owner ? owner->GetOwner() : nullptr;

			if (scene)
			{
				scene->SetCanvasReferenceSize(&canvas, referenceSize);
			}
		}
	}

	EditorUI::EndPropertyGrid();
}
