#include "HierarchyPanel.h"
#include "Engine/Actor/Actor.h"
#include "imgui.h"

void HierarchyPanel::Render(SceneBase* scene, const Callbacks& callbacks)
{
    if (ImGui::Begin("Hierarchy"))
    {
        if (scene)
        {
            for (auto* actor : scene->GetRootActors())
            {
                RenderActorNode(actor);
            }
        }

        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
        {
            // Right-clicking on empty space opens the context menu for creating a new actor
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                m_showMenuPopup = true;
            }

            // Clicking on empty space deselects the current actor
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_pSelectedActor = nullptr;
            }
        }
        ImGui::End();
    }

    // Menu popup
    if (m_showMenuPopup)
    {
        ImGui::OpenPopup("HierarchyContextMenu");
        m_showMenuPopup = false;
    }

    if (ImGui::BeginPopup("HierarchyContextMenu"))
    {
        // Menu item for creating a new actor
        if (ImGui::MenuItem("Create Empty Actor"))
        {
            m_showActorCreationPopup = true;
            m_newActorNameBuffer[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        // ==========================================================
        // Å´Å´ Additional context menu items can be added here Å´Å´
        // ==========================================================

        ImGui::EndPopup();
    }

    // Handle the Create Actor popup
    if (m_showActorCreationPopup)
    {
        ImGui::OpenPopup("Create Actor");
        m_showActorCreationPopup = false;
    }

    // The popup is modal, so it will block interaction with the rest of the UI until closed.
    if (ImGui::BeginPopupModal("Create Actor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Actor Name:");
        ImGui::InputText("##NewActorName", m_newActorNameBuffer, sizeof(m_newActorNameBuffer));

        ImGui::Separator();

        // Create button triggers the callback to create the script
        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            std::string name = m_newActorNameBuffer;
            if (!name.empty() && callbacks.onCreateActor)
            {
                callbacks.onCreateActor(name);
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

    // Handle the Delete Actor confirmation popup
    if (m_pActorToDelete)
    {
        ImGui::OpenPopup("Confirm Delete Actor");
    }


    // This ia also a modal popup
    if (ImGui::BeginPopupModal("Confirm Delete Actor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Do you want to delete the following actor?");
        ImGui::Text("%s", m_pSelectedActor ? m_pSelectedActor->GetName().c_str() : "Unknown");
        ImGui::Separator();

		// Delete button triggers the callback to delete the actor
        if (ImGui::Button("Delete", ImVec2(120, 0)))
        {
            if (m_pSelectedActor && callbacks.onDeleteActor)
            {
                callbacks.onDeleteActor(m_pActorToDelete);
                if (m_pSelectedActor == m_pActorToDelete)
                    m_pSelectedActor = nullptr;
                m_pActorToDelete = nullptr;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        // Cancel button just closes the popup without doing anything
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_pActorToDelete = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void HierarchyPanel::RenderActorNode(Actor* actor)
{
    if (!actor) return;

    auto children = actor->GetDirectChildren();
    bool hasChildren = !children.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (m_pSelectedActor == actor)
        flags |= ImGuiTreeNodeFlags_Selected;

    bool opened = ImGui::TreeNodeEx(
        (void*)actor,
        flags,
        "%s",
        actor->GetName().c_str()
    );

    // Left-click to select
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        m_pSelectedActor = actor;
    }

	// Right-click to open the context menu for this actor
    std::string popupId = "ActorContextMenu_" + actor->GetName();
    if (ImGui::BeginPopupContextItem(popupId.c_str()))
    {
        m_pSelectedActor = actor;

        if (ImGui::MenuItem("Delete Actor"))
        {
            m_pActorToDelete = actor;
        }

        // ==========================================================
        // Å´Å´ Additional context menu items can be added here Å´Å´
        // ==========================================================


        ImGui::EndPopup();
    }
    if (opened)
    {
        for (auto* child : children)
        {
            RenderActorNode(child);
        }
        ImGui::TreePop();
    }
}
