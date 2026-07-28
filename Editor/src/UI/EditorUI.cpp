#include "UI/EditorUI.h"
#include <cfloat>
#include "UI/EditorTheme.h"
#include "imgui.h"
#include "Engine/Resource/AssetManager.h"
#include "UI/Inspector/AssetPicker.h"

namespace
{
	// Push a string_view as an ImGui ID to ensure uniqueness
	void PushStringViewId(std::string_view id)
	{
		ImGui::PushID(id.data(), id.data() + id.size());
	}

	// Pop the last pushed ImGui ID
	void BeginPropertyRow(std::string_view label)
	{
		// Start a new row in the property grid
		ImGui::TableNextRow();
		
		// Label column
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();

		ImGui::TextUnformatted(label.data(), label.data() + label.size());

		// Value column
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);

		// Use the label as the ID for the value field to ensure uniqueness
		PushStringViewId(label);
	}

	// Pop the last pushed ImGui ID after finishing the property row
	void EndPropertyRow()
	{
		ImGui::PopID();
	}
}

bool EditorUI::BeginPropertyGrid(std::string_view id)
{
	// Push the ID for the property grid to ensure uniqueness
	PushStringViewId(id);

	// Define the table flags for the property grid
	const ImGuiTableFlags flag =
		ImGuiTableFlags_SizingStretchProp	|
		ImGuiTableFlags_NoSavedSettings		|
		ImGuiTableFlags_PadOuterX;

	// Begin the table with 2 columns for the property grid
	const bool opend = ImGui::BeginTable("##PropertyGrid", 2, flag);

	if (!opend)
	{
		// BeginTable did not open, so there will be no matching
		// EndPropertyGrid call to restore the grid ID.
		ImGui::PopID();
		return false;
	}

	// Setup the columns for the property grid
	ImGui::TableSetupColumn(
		"Property", 
		ImGuiTableColumnFlags_WidthFixed, 
		EditorTheme::Metrics::PropertyLabelWidth
	);

	// The second column will stretch to fill the remaining space
	ImGui::TableSetupColumn(
		"Value",
		ImGuiTableColumnFlags_WidthStretch
	);

	return true;
}

void EditorUI::EndPropertyGrid()
{
	ImGui::EndTable();

	// Match the grid ID pushed by BeginPropertyGrid.
	ImGui::PopID();
}

bool EditorUI::BoolField(std::string_view label, bool& value)
{
	BeginPropertyRow(label);

	// Render the checkbox for the boolean value
	const bool changed = ImGui::Checkbox("##Value", &value);

	EndPropertyRow();
	return changed;
}

bool EditorUI::FloatField(std::string_view label, float& value, float speed)
{
	BeginPropertyRow(label);

	// Render the drag float field for the float value
	const bool changed = ImGui::DragFloat("##Value", &value, speed);

	EndPropertyRow();
	return changed;
}

bool EditorUI::UIntField(std::string_view label, unsigned int& value, float speed)
{
	BeginPropertyRow(label);

	const bool changed = ImGui::DragScalar(
		"##Value",
		ImGuiDataType_U32,
		&value,
		speed
	);

	EndPropertyRow();
	return changed;
}

bool EditorUI::Vector2Field(std::string_view label, Vector2& value, float speed)
{
	BeginPropertyRow(label);

	const bool changed = ImGui::DragFloat2("##Value", &value.x, speed);

	EndPropertyRow();
	return changed;
}

bool EditorUI::Vector3Field(std::string_view label, Vector3& value, float speed)
{
	BeginPropertyRow(label);

	// Render the drag float fields for the Vector3 components
	const bool changed = ImGui::DragFloat3("##Value", &value.x, speed);

	EndPropertyRow();
	return changed;
}

bool EditorUI::ColorField(std::string_view label, Vector4& value)
{
	BeginPropertyRow(label);

	// Render the color edit field for the Vector4 color value
	const bool changed = ImGui::ColorEdit4("##Value", &value.x, ImGuiColorEditFlags_AlphaBar);

	EndPropertyRow();
	return changed;
}

bool EditorUI::ComboField(
	std::string_view label,
	int& currentIndex,
	const char* const items[],
	int itemCount
)
{
	BeginPropertyRow(label);

	const bool changed = ImGui::Combo(
		"##Value",
		&currentIndex,
		items,
		itemCount
	);

	EndPropertyRow();
	return changed;
}

bool EditorUI::AssetField(
	std::string_view label,
	const AssetManager& assetManager,
	AssetType assetType,
	const Guid& currentAssetId,
	Guid& outSelectedAssetId
)
{
	BeginPropertyRow(label);

	// Render the asset picker for selecting an asset
	const bool changed = AssetPicker::Draw(
		"##Value",
		assetManager,
		assetType,
		currentAssetId,
		outSelectedAssetId
	);

	EndPropertyRow();
	return changed;
}
