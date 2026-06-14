#include "HierarchyPanel.h"
#include "Engine/Actor/Actor.h"
#include "imgui.h"

void HierarchyPanel::Render(SceneBase* scene)
{
    ImGui::Begin("Hierarchy");

    if (scene)
    {
        for (auto* actor : scene->GetRootActors())
        {
            RenderActorNode(actor);
        }
    }

    // Clicking on empty space deselects the current actor
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_pSelectedActor = nullptr;
    }

    ImGui::End();
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

    // Click to select
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        m_pSelectedActor = actor;
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
