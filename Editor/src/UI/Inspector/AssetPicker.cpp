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
		if (!currentAssetId.IsValid())
		{
			return "<None>";
		}

		const AssetEntry* entry = assetManager.GetAssetEntry(currentAssetId);

		if (!entry)
		{
			return "<Missing Asset>";
		}

		if (entry->type != expectedType)
		{
			return "<Invalid Asset Type>";
		}

		outStorage = entry->relativePath;
		return outStorage.c_str();
	}
}

bool AssetPicker::TrySelectPayload(
	const AssetManager& assetManager,
	AssetType expectedType,
	const EditorAssetDragDropPayload& payload,
	const Guid& currentAssetId,
	Guid& outSelectedAssetId)
{
	outSelectedAssetId = currentAssetId;

	if (!payload.assetGuid.IsValid() || payload.assetType != expectedType)
	{
		return false;
	}

	const AssetEntry* entry = assetManager.GetAssetEntry(payload.assetGuid);

	if (!entry || entry->type != expectedType || entry->guid == currentAssetId)
	{
		return false;
	}

	outSelectedAssetId = entry->guid;
	return true;
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

	const bool noneSelected = !currentAssetId.IsValid();

	if (ImGui::BeginCombo(label, previewText))
	{
		// Draw the "<None>" option to allow deselecting the current asset
		if (ImGui::Selectable("<None>", noneSelected))
		{
			if (currentAssetId.IsValid())
			{
				outSelectedAssetId = Guid{};
				changed = true;
			}
		}

		if (noneSelected)
		{
			ImGui::SetItemDefaultFocus();
		}

		// Get all asset entries registered with the AssetManager for the specified asset type
		const std::vector<AssetEntry> entries = assetManager.GetAssetEntries(assetType);

		// Draw the list of assets in the combo box
		if (entries.empty())
		{
			ImGui::Separator();
			ImGui::TextDisabled("No assets found.");
		}
		else
		{
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

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
		}

		ImGui::EndCombo();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* raw = ImGui::AcceptDragDropPayload(EditorAssetDragDropPayloadType);
			raw && raw->DataSize == sizeof(EditorAssetDragDropPayload))
		{
			const auto& payload = *static_cast<const EditorAssetDragDropPayload*>(raw->Data);
			Guid selected;

			if (TrySelectPayload(assetManager, assetType, payload, currentAssetId, selected))
			{
				outSelectedAssetId = selected;
				changed = true;
			}
		}

		ImGui::EndDragDropTarget();
	}

	return changed;
}
