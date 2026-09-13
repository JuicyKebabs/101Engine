#include "ActorImprintsPanel.h"

#include "ActorImprint/ActorImprintAssetWorkflow.h"
#include "Engine/Resource/AssetManager.h"
#include "UI/AssetDragDropPayload.h"
#include "UI/EditorUI.h"
#include "imgui.h"

#include <algorithm>

std::vector<AssetEntry> ActorImprintsPanel::GetVisibleEntries(const AssetManager& assets)
{
	auto entries = assets.GetAssetEntries(AssetType::ActorImprint);

	std::erase_if(entries, [](const AssetEntry& entry)
	{
			// Filter out any ActorImprint assets that are not in the managed directory
		return !ActorImprintAssetWorkflow::IsManagedAssetPath(entry.relativePath);
	});
	return entries;
}

void ActorImprintsPanel::Render(const AssetManager& assets, const Callbacks& callbacks)
{
	const bool visible = ImGui::Begin("Actor Imprints");
	if (visible)
	{
		//{
		//	EditorUI::DisabledScope disabled(!callbacks.canModify);
		//	if (ImGui::Button("Create") && callbacks.canModify) RequestCreateDialog();
		//}
		//ImGui::SameLine();
		//const bool hasSelection = m_selectedAssetGuid.IsValid() &&
		//	assets.GetAssetEntry(m_selectedAssetGuid) != nullptr;
		//{
		//	EditorUI::DisabledScope disabled(!hasSelection || !callbacks.canModify);
		//	if (ImGui::Button("Edit")) DispatchEdit(callbacks, m_selectedAssetGuid);
		//	ImGui::SameLine();
		//	if (ImGui::Button("Delete"))
		//		RequestDeleteConfirmation(m_selectedAssetGuid);
		//}
		//ImGui::Separator();

		const auto entries = GetVisibleEntries(assets);	// Get all ActorImprint entries and filter to only managed ones

		// Right click menu
		for (const AssetEntry& entry : entries)
		{
			ImGui::PushID(entry.guid.ToString().c_str());
			const bool selected = entry.guid == m_selectedAssetGuid;
			if (ImGui::Selectable(entry.relativePath.c_str(), selected))
				m_selectedAssetGuid = entry.guid;
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
				callbacks.canModify)
				DispatchEdit(callbacks, entry.guid);
			if (ImGui::BeginPopupContextItem("ActorImprintItemContext"))
			{
				if (ImGui::MenuItem("Edit", nullptr, false, callbacks.canModify))
					DispatchEdit(callbacks, entry.guid);
				if (ImGui::MenuItem("Delete", nullptr, false, callbacks.canModify))
					RequestDeleteConfirmation(entry.guid);
				ImGui::EndPopup();
			}
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				const EditorAssetDragDropPayload payload{entry.guid, entry.type};
				ImGui::SetDragDropPayload(EditorAssetDragDropPayloadType, &payload, sizeof(payload));
				ImGui::TextUnformatted(entry.relativePath.c_str());
				ImGui::EndDragDropSource();
			}
			ImGui::PopID();
		}

		// Log message to indicate that there are no ActorImprint assets if the list is empty
		if (entries.empty()) ImGui::TextDisabled("No ActorImprint assets found.");


		if (ImGui::BeginPopupContextWindow("ActorImprintsEmptyContext",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Actor Imprint...", nullptr, false, callbacks.canModify))
				RequestCreateDialog();
			ImGui::EndPopup();
		}

		if (!m_diagnostic.empty())
		{
			ImGui::Separator();
			ImGui::TextWrapped("%s", m_diagnostic.c_str());
		}
	}
	ImGui::End();

	if (m_openCreatePopup)
	{
		ImGui::OpenPopup("Create ActorImprint");
		m_openCreatePopup = false;
		m_name[0] = '\0';
	}
	if (ImGui::BeginPopupModal("Create ActorImprint", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Name", m_name, sizeof(m_name));
		if (ImGui::Button("Create", ImVec2(120, 0)) && DispatchCreate(callbacks, m_name))
			ImGui::CloseCurrentPopup();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (m_openDeletePopup)
	{
		ImGui::OpenPopup("Delete ActorImprint");
		m_openDeletePopup = false;
	}
	if (ImGui::BeginPopupModal("Delete ActorImprint", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const AssetEntry* entry = assets.GetAssetEntry(m_pendingDeleteGuid);
		ImGui::Text("Delete '%s'?", entry ? entry->relativePath.c_str() : "Unknown");
		ImGui::TextUnformatted("This action cannot be undone.");
		if (ImGui::Button("Delete", ImVec2(120, 0)))
		{
			if (DispatchDelete(callbacks, m_pendingDeleteGuid))
			{
				if (m_selectedAssetGuid == m_pendingDeleteGuid) m_selectedAssetGuid = {};
				m_pendingDeleteGuid = {};
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			m_pendingDeleteGuid = {};
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
