#include "UI/Inspector/Components/MeshRendererInspector.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Resource/AssetManager.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void MeshRendererInspector::Draw(MeshRenderer& meshRenderer, const InspectorContext& context)
{
	Guid selectedMeshId = meshRenderer.GetAssetId();
	bool visible = meshRenderer.IsVisible();
	Vector4 color = meshRenderer.GetColor();

	// Property grid
	if (!EditorUI::BeginPropertyGrid("MeshRendererProperties")) return;

	// Draw mesh asset field
	if (context.assetManager)
	{
		const Guid currentMeshId = meshRenderer.GetAssetId();

		if (EditorUI::AssetField("Mesh", *context.assetManager, AssetType::Mesh, currentMeshId, selectedMeshId))
		{
			meshRenderer.SetMeshAsset(selectedMeshId);
		}
	}

	// Draw visibility field
	if (EditorUI::BoolField("Visible", visible))
	{
		meshRenderer.SetVisible(visible);
	}

	// Draw color field
	if (EditorUI::ColorField("Color", color))
	{
		meshRenderer.SetColor(color);
	}

	// Draw sort order field if the renderer is in screen space
	// (In the Screen-Space UI hierarchy)
	if (meshRenderer.GetRenderSpace() == RenderSpace::Screen)
	{
		UINT sortOrder = meshRenderer.GetSortOrderInCanvas();

		if (EditorUI::UIntField("Sort Order", sortOrder))
		{
			meshRenderer.SetSortOrderInCanvas(sortOrder);
		}
	}

	EditorUI::EndPropertyGrid();
}