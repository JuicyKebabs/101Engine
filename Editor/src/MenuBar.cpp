#include "MenuBar.h"
#include "imgui.h"

void MenuBar::Render(const Callbacks& callbacks)
{
    if (ImGui::BeginMainMenuBar())
    {
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
            ImGui::Separator();
            if (ImGui::MenuItem("Build Game"))
            {
                if (callbacks.onBuildGame) callbacks.onBuildGame();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Test: Unregister Game Components"))
            {
                if (callbacks.onTest) callbacks.onTest();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::MenuItem("Create Behavior..."))
            {
                m_showCreateBehaviorPopup = true;
                m_newBehaviorNameBuffer[0] = '\0';
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (m_showCreateBehaviorPopup)
    {
        ImGui::OpenPopup("Create Behavior");
        m_showCreateBehaviorPopup = false;
    }

    if (ImGui::BeginPopupModal("Create Behavior", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Class Name:");
        ImGui::InputText("##NewBehaviorName", m_newBehaviorNameBuffer, sizeof(m_newBehaviorNameBuffer));

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            std::string name = m_newBehaviorNameBuffer;
            if (!name.empty() && callbacks.onCreateBehavior)
            {
                callbacks.onCreateBehavior(name);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
