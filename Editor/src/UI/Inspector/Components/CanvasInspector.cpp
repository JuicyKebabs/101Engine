#include "UI/Inspector/Components/CanvasInspector.h"
#include "Engine/UI/Canvas.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include <algorithm>

void CanvasInspector::Draw(Canvas& canvas, const InspectorContext&)
{
	Actor* owner = canvas.GetOwner();
	SceneBase* scene = owner ? owner->GetOwner() : nullptr;

	if (!scene)
	{
		DBG("CanvasInspector::Draw: Canvas owner is not part of a scene.");
		return;
	}

	bool visible = canvas.IsVisible();
	UINT sortOrder = canvas.GetSortOrder();

	if (!EditorUI::BeginPropertyGrid("CanvasProperties")) return;

	// Visibility
	if (EditorUI::BoolField("Visible", visible)) canvas.SetVisible(visible);

	// Only a root Canvas owns the render mode used by its hierarchy.
	if (canvas.IsRootCanvas())
	{
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
			scene->SetCanvasRenderMode(&canvas, renderMode);
		}
	}

	// Scale Mode (only for Screen Space canvases)
	if (canvas.IsRootCanvas())
	{
		Vector2 referenceSize = canvas.GetReferenceSize();

		if (canvas.GetRenderMode() == CanvasRenderMode::ScreenSpace)
		{// In case of Screen - Space Canvas, display the Scale Mode options
			int scaleMode = static_cast<int>(canvas.GetScaleMode());

			const char* scaleModes[] =
			{
				"Constant Pixel Size",
				"Scale With Screen Size"
			};

			// Scale Mode selectable pull-down
			if (EditorUI::ComboField("Scale Mode", scaleMode, scaleModes, static_cast<int>(CanvasScaleMode::Max)))
			{
				if (scene)
				{
					scene->SetCanvasScaleMode(&canvas, static_cast<CanvasScaleMode>(scaleMode));
				}
			}

			// Scale With Screen Size options (Reference Resolution and Match Width Or Height)
			if (canvas.GetScaleMode() == CanvasScaleMode::ScaleWithScreenSize)
			{
				// Reference Resolution field
				if (EditorUI::Vector2Field("Reference Resolution", referenceSize, 1.0f))
				{
					if (scene)
					{
						scene->SetCanvasReferenceSize(&canvas, referenceSize);
					}
				}

				// Match Width Or Height field
				float match = canvas.GetMatchWidthOrHeight();

				if (EditorUI::FloatField("Match Width Or Height", match, 0.01f))
				{
					match = std::clamp(match, 0.0f, 1.0f);

					if (scene)
					{
						scene->SetCanvasMatchWidthOrHeight(&canvas, match);
					}
				}
			}
		}
		else
		{// In case of World - Space Canvas, display the Reference Size options
			if (EditorUI::Vector2Field("Reference Size", referenceSize, 1.0f))
			{
				if (scene)
				{
					scene->SetCanvasReferenceSize(&canvas, referenceSize);
				}
			}
		}
	}

	// Sort Order
	if (EditorUI::UIntField("Sort Order", sortOrder)) canvas.SetSortOrder(sortOrder);
	
	EditorUI::EndPropertyGrid();
}
