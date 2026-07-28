#include "AssetPicker.h"
#include "imgui.h"

namespace
{
	// Helper function to get the preview text for the currently selected asset
	const char* GetPreviewText(
		const AssetManager& assetManager,
		AssetType expectedType,
		const Guid& currentAssetId,
		std::string& outStorage
	)
	{
		if (!currentAssetId.IsValid()) return "<None>";

		const AssetEntry* entry = assetManager.GetAssetEntry(currentAssetId);

		if (!entry) return "<Missing Asset>";

		if (entry->type != expectedType) return "<Invalid Asset Type>";

		outStorage = entry->relativePath;
		return outStorage.c_str();
	}
}

bool AssetPicker::Draw(
	const char* label,
	const AssetManager& assetManager,
	AssetType assetType,
	const Guid& currentAssetId,
	Guid& outSelectedAssetId
)
{
	outSelectedAssetId = currentAssetId; // Default to the current asset ID

	// Get preview text for the currently selected asset
	std::string previewStorage;

	const char* previewText = GetPreviewText(
		assetManager, 
		assetType, 
		currentAssetId, 
		previewStorage
	);

	bool changed = false;

	// No change if the combo box is not open
	if (!ImGui::BeginCombo(label, previewText)) return changed;

	const bool noneSelected = !currentAssetId.IsValid();

	// Draw the "<None>" option to allow deselecting the current asset
	if (ImGui::Selectable("<None>", noneSelected))
	{
		if (currentAssetId.IsValid())
		{
			outSelectedAssetId = Guid{};
			changed = true;
		}
	}

	if (noneSelected) ImGui::SetItemDefaultFocus();

	// Get all asset entries registered with the AssetManager for the specified asset type
	const std::vector<AssetEntry> entries = assetManager.GetAssetEntries(assetType);

	// Draw the list of assets in the combo box
	if (entries.empty())
	{// No asset entries found for the specified type
		ImGui::Separator();
		ImGui::TextDisabled("No assets found.");
	}
	else
	{// Draw every asset entry as a selectable item in the combo box 
		ImGui::Separator();

		for (const AssetEntry& entry : entries)
		{
			const bool selected = (entry.guid == currentAssetId);

			if (ImGui::Selectable(entry.relativePath.c_str(), selected))
			{
				if (entry.guid != currentAssetId)
				{
					outSelectedAssetId = entry.guid;
					changed = true;
				}
			}

			// Focus the currently selected item when the combo box is opened
			if (selected) ImGui::SetItemDefaultFocus();
		}
	}

	ImGui::EndCombo();
	return changed;
}