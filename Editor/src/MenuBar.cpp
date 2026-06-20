#include "MenuBar.h"
#include "imgui.h"

void MenuBar::Render(const Callbacks& callbacks)
{
    if (ImGui::BeginMainMenuBar())
    {
		// File menu for scene management (new/open/save)
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene"))
            {
                if (callbacks.onNewScene) callbacks.onNewScene();
            }
            if (ImGui::MenuItem("Open Scene"))
            {
                if (callbacks.onOpenScene) callbacks.onOpenScene();
            }
            if (ImGui::MenuItem("Save Scene"))
            {
                if (callbacks.onSaveScene) callbacks.onSaveScene();
            }
            ImGui::EndMenu();
        }

		// Assets menu for creating new assets like behaviors
        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::MenuItem("Create Script..."))
            {
                m_showCreateScriptPopup = true;
                m_newScriptNameBuffer[0] = '\0';
                m_createAsBehavior = true;
            }
            ImGui::EndMenu();
        }

		// Build menu for building the game and hot-reloading game code
        if (ImGui::BeginMenu("Build"))
        {
            if (ImGui::MenuItem("Build Game"))
            {
                if (callbacks.onBuildGame) callbacks.onBuildGame();
            }
            if (ImGui::MenuItem("Reload GameCode"))
            {
                // Build without reconfigure
                if (callbacks.onReloadGameCode) callbacks.onReloadGameCode(false);
            }
            if (ImGui::MenuItem("Reload GameCode (with Reconfigure)"))
            {
                // Build with reconfigure
                if (callbacks.onReloadGameCode) callbacks.onReloadGameCode(true);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

	// Handle the Create Script popup
    if (m_showCreateScriptPopup)
    {
        ImGui::OpenPopup("Create Script");
        m_showCreateScriptPopup = false;
    }

	// The popup is modal, so it will block interaction with the rest of the UI until closed.
    if (ImGui::BeginPopupModal("Create Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Class Name:");
        ImGui::InputText("##NewScriptName", m_newScriptNameBuffer, sizeof(m_newScriptNameBuffer));

        ImGui::Separator();
        ImGui::Text("Type:");

		// Radio button to create Behavior Component
        if (ImGui::RadioButton("Behavior Component", m_createAsBehavior))
        {
            m_createAsBehavior = true;
        }
        ImGui::SameLine();

		// Radio button to create Plain Class
        if (ImGui::RadioButton("Plain Class", !m_createAsBehavior))
        {
            m_createAsBehavior = false;
        }

        ImGui::Separator();

		// Create button triggers the callback to create the script
        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            std::string name = m_newScriptNameBuffer;
            if (!name.empty() && callbacks.onCreateScript)
            {
                callbacks.onCreateScript(name, m_createAsBehavior);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

		// Cancel button just closes the popup without doing anything
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
