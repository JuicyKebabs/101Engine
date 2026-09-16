#include "SceneAssetPanel.h"

#include "Scene/SceneAssetWorkflow.h"
#include "UI/EditorUI.h"
#include "imgui.h"

#include <filesystem>

std::vector<AssetEntry> SceneAssetPanel::GetVisibleEntries(const AssetManager& assets)
{
	auto entries = assets.GetAssetEntries(AssetType::Scene);
	std::erase_if(entries, [](const AssetEntry& entry)
	{
		return !SceneAssetWorkflow::IsManagedAssetPath(entry.relativePath);
	});
	return entries;
}

void SceneAssetPanel::Render(
	const AssetManager& assets, 
	const Guid& editorStartupSceneGuid,
	const Guid& gameStartupSceneGuid,
	const Callbacks& callbacks)
{
	if (ImGui::Begin("Scenes"))
	{
		const auto entries = GetVisibleEntries(assets);
		for (const AssetEntry& entry : entries)
		{
			ImGui::PushID(entry.guid.ToString().c_str());
			const bool isStartup = entry.guid == editorStartupSceneGuid;
			const std::string label = std::string(isStartup ? "* " : "  ") + entry.relativePath;
			if (ImGui::Selectable(label.c_str(), entry.guid == m_selectedGuid)) m_selectedGuid = entry.guid;
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && callbacks.canModify && callbacks.onOpen)
				callbacks.onOpen(entry.guid);
			if (ImGui::BeginPopupContextItem("SceneItemContext"))
			{
				if (ImGui::MenuItem("Open", nullptr, false, callbacks.canModify) && callbacks.onOpen)
				{
					callbacks.onOpen(entry.guid);
				}

				if (ImGui::MenuItem("Rename...", nullptr, false, callbacks.canModify))
				{
					m_renameGuid = entry.guid;
					const std::string stem = std::filesystem::path(entry.relativePath).stem().string();
					strncpy_s(m_name, stem.c_str(), _TRUNCATE);
					m_openRenamePopup = true;
				}

				if (ImGui::MenuItem("Set as Editor Startup Scene", nullptr, isStartup,
					callbacks.canModify && !isStartup) && callbacks.onSetEditorStartup)
				{
					callbacks.onSetEditorStartup(entry.guid);
				}

				if (ImGui::MenuItem("Set as Game Startup Scene", nullptr, entry.guid == gameStartupSceneGuid,
					callbacks.canModify && entry.guid != gameStartupSceneGuid) && callbacks.onSetGameStartup)
				{
					callbacks.onSetGameStartup(entry.guid);
				}

				ImGui::EndPopup();
			}
			ImGui::PopID();
		}

		if (entries.empty())
		{
			ImGui::TextDisabled("No Scene assets found.");
		}

		if (ImGui::BeginPopupContextWindow("ScenesEmptyContext",
			ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Scene...", nullptr, false, callbacks.canModify)) m_openCreatePopup = true;
			{
				ImGui::EndPopup();
			}
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
		ImGui::OpenPopup("Create Scene");
		m_openCreatePopup = false;
		m_name[0] = '\0';
	}
	if (ImGui::BeginPopupModal("Create Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Name", m_name, sizeof(m_name));

		if (ImGui::Button("Create", ImVec2(120, 0)) && callbacks.onCreate && callbacks.canModify && callbacks.onCreate(m_name))
			{
				ImGui::CloseCurrentPopup();
			}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }

		ImGui::EndPopup();
	}

	if (m_openRenamePopup) 
	{ 
		ImGui::OpenPopup("Rename Scene"); 
		m_openRenamePopup = false; 
	}

	if (ImGui::BeginPopupModal("Rename Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Name", m_name, sizeof(m_name));

		if (ImGui::Button("Rename", ImVec2(120, 0)) && callbacks.onRename && callbacks.canModify &&
			callbacks.onRename(m_renameGuid, m_name))
		{
			m_renameGuid = {};
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0))) 
		{ 
			m_renameGuid = {};
			ImGui::CloseCurrentPopup();
		}
		
		ImGui::EndPopup();
	}
}
