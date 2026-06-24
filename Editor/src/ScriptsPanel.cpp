#include "ScriptsPanel.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Scene/SceneBase.h"	
#include "imgui.h"
#include <filesystem>

namespace fs = std::filesystem;

void ScriptsPanel::Render(const Callbacks& callbacks, SceneBase* scene)
{
	ImGui::Begin("Scripts");

	auto scripts = ScanScripts();

	if (scripts.empty())
	{
		ImGui::TextDisabled("No scripts found in Game/GameCode/");
	}
	else
	{
		ImGui::Text("Scripts (%zu):", scripts.size());
		ImGui::Separator();

		for (const auto& script : scripts)
		{
			// Type label
			ImGui::TextDisabled(script.isBehavior ? "[Behavior]" : "[Class]  ");
			ImGui::SameLine();

			// Script name
			ImGui::Text("%s", script.name.c_str());
			ImGui::SameLine();

			// Open button
			std::string openId = "Open##" + script.name;
			if (ImGui::SmallButton(openId.c_str()))
			{
				if (callbacks.onOpen) callbacks.onOpen(script.name);
			}
			ImGui::SameLine();

			// Delete button
			std::string deleteId = "Delete##" + script.name;
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
			if (ImGui::SmallButton(deleteId.c_str()))
			{
				m_pendingDeleteName = script.name;
				m_showDeleteConfirm = true;
				m_inUseByScene = false;

				// Check if the script is in use by any actors in the scene
				if (script.isBehavior && scene)
				{
					for (const auto& actor : scene->GetAllActors())
					{
						// Check all actor's components
						for (const auto& typeId : actor->GetComponentsTypeIds())
						{
							// Convert type index to component name that is registered in ComponentRegistry
							std::string componentname = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

							// Check if the component name matches the script name
							if (componentname == script.name)
							{
								m_inUseByScene = true;
								break;
							}
						}
						if (m_inUseByScene) break;
					}
				}
			}
			ImGui::PopStyleColor();
		}
	}


	// Popup for delete confirmation
	if (m_showDeleteConfirm)
	{
		ImGui::OpenPopup("Confirm Delete");
		m_showDeleteConfirm = false; // Reset the flag to avoid reopening the popup
	}

	// Render the delete confirmation popup
	if (ImGui::BeginPopupModal("Confirm Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// Check if the script is in use by actors in the scene
		if (m_inUseByScene)
		{// If the script is in use by actors in the scene, show a warning message
			ImGui::TextColored(
				ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
				"Warning: '%s' is currently attached to actors in the scene.",
				m_pendingDeleteName.c_str());
			ImGui::Text(
				"Those components will be lost after deletion.");
			ImGui::Separator();
		}

		// If the script is not in use, ask for confirmation
		ImGui::Text("Delete '%s' (.h and .cpp)?", m_pendingDeleteName.c_str());
		ImGui::Text("This action cannot be undone.");
		ImGui::Separator();

		// Delete button(Red)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button("Delete", ImVec2(120, 0)))
		{
			if (callbacks.onDelete) callbacks.onDelete(m_pendingDeleteName);
			ImGui::CloseCurrentPopup();
		}

		ImGui::PopStyleColor(2);
		ImGui::SameLine();

		// Cancel button
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			m_pendingDeleteName.clear();
			m_inUseByScene = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::End();
}

std::vector<ScriptsPanel::ScriptEntry> ScriptsPanel::ScanScripts()
{
	std::vector<ScriptEntry> entries;

	// Resolve the GameCode directory path
	std::string gameCodeDir = PathManager::Resolve("Game/GameCode");

	// Check if GameCode directory exists
	if (!fs::exists(gameCodeDir)) return entries;

	// Exclude certain scripts from being displayed in the panel (e.g. sample scripts)
	static const std::vector<std::string> kExcluded = {"SampleScene"};

	for (const auto& entry : fs::directory_iterator(gameCodeDir))
	{
		// Skip if not a header file
		if (entry.path().extension() != ".h") continue;

		// Get name without extension
		std::string name = entry.path().stem().string();

		// Apply exclusion filter
		bool execlude = false;
		for (const auto& ex : kExcluded)
		{
			if (name == ex)
			{
				execlude = true;
				break;
			}
		}
		if (execlude) continue;

		// Check if the script is registered in ComponentRegistry
		bool isBehavior = ComponentRegistry::Get().Has(name);

		// Add the script entry to the list
		entries.push_back({ name, isBehavior });
	}

	return entries;
}