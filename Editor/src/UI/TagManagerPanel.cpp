#include "TagManagerPanel.h"

#include "imgui.h"
#include <cstring>

void TagManagerPanel::OpenNamePopup(const char* title, std::string current)
{
	m_popupTitle = title;
	m_target = current;
	strncpy_s(m_nameBuffer, current.c_str(), _TRUNCATE);
	m_openNamePopup = true;
}

void TagManagerPanel::Render(const std::vector<std::string>& userTags, const Callbacks& callbacks)
{
	ImGui::Begin("Tags");

	if (ImGui::Button("Create Tag..."))
	{
		OpenNamePopup("Create Tag");
	}

	ImGui::Separator();
	ImGui::TextDisabled("None (Reserved)");
	ImGui::TextDisabled("MainCamera (Reserved)");
	ImGui::TextDisabled("InitialSky (Reserved)");

	for (const std::string& name : userTags)
	{
		ImGui::PushID(name.c_str());
		ImGui::Selectable(name.c_str());

		if (ImGui::BeginPopupContextItem("TagActions"))
		{
			if (ImGui::MenuItem("Rename..."))
			{
				OpenNamePopup("Rename Tag", name);
			}

			if (ImGui::MenuItem("Delete..."))
			{
				m_target = name;
				m_openDeletePopup = true;
			}

			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	ImGui::End();

	if (m_openNamePopup)
	{
		ImGui::OpenPopup(m_popupTitle.c_str());
		m_openNamePopup = false;
	}

	if (ImGui::BeginPopupModal(m_popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Name", m_nameBuffer, sizeof(m_nameBuffer));

		if (ImGui::Button("OK"))
		{
			bool succeeded;

			if (m_target.empty())
			{
				succeeded = callbacks.onCreate && callbacks.onCreate(m_nameBuffer);
			}
			else
			{
				succeeded = callbacks.onRename && callbacks.onRename(m_target, m_nameBuffer);
			}

			if (succeeded)
			{
				m_target.clear();
				m_nameBuffer[0] = '\0';
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel"))
		{
			m_target.clear();
			m_nameBuffer[0] = '\0';
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if (m_openDeletePopup)
	{
		ImGui::OpenPopup("Delete Tag");
		m_openDeletePopup = false;
	}

	if (ImGui::BeginPopupModal("Delete Tag", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("Delete Tag '%s'?", m_target.c_str());

		if (ImGui::Button("Delete"))
		{
			if (callbacks.onDelete && callbacks.onDelete(m_target))
			{
				m_target.clear();
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel"))
		{
			m_target.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}
