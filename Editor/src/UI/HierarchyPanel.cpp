#include "HierarchyPanel.h"
#include "Engine/Actor/Actor.h"
#include "imgui.h"
#include <cstdio>

Actor* HierarchyPanel::GetSelectedActor(SceneBase* scene)
{
	if (!scene || !m_selectedActorGuid.IsValid()) return nullptr;

	Actor* selectedActor = scene->ResolveActor(m_selectedActorGuid);

	if (!selectedActor || selectedActor->IsDestroyed())
	{
		m_selectedActorGuid = {};
		return nullptr;
	}

	return selectedActor;
}

void HierarchyPanel::Render(SceneBase* scene, const Callbacks& callbacks)
{
    if (ImGui::Begin("Hierarchy"))
    {
        if (scene)
        {
            // Always provide an explicit destination for moving an Actor
            // out of its current parent hierarchy.
            RenderRootDropTarget(callbacks);

            for (auto* actor : scene->GetRootActors())
            {
                RenderActorNode(actor, scene, callbacks);
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
				m_selectedActorGuid = {};
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
			m_creationParentGuid = {};
            m_showActorCreationPopup = true;
            m_newActorNameBuffer[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        // ==========================================================
		// Additional context menu items can be added here.
        // ==========================================================

        ImGui::EndPopup();
    }

	// Handle the Rename Actor popup
	if (m_showRenamePopup)
	{
		ImGui::OpenPopup("Rename Actor");
		m_showRenamePopup = false;
	}

    if (ImGui::BeginPopupModal("Rename Actor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Actor Name:");
        ImGui::InputText("##RenameActorName", m_renameBuffer, sizeof(m_renameBuffer));

        ImGui::Separator();

		// Rename button triggers the rename action
        if (ImGui::Button("Rename", ImVec2(120, 0)))
        {
            std::string name = m_renameBuffer;
			bool renamed = false;

            if (!name.empty() && callbacks.onRenameActor)
            {
				renamed = callbacks.onRenameActor(m_renameTargetGuid, name);
            }

			if (renamed) ImGui::CloseCurrentPopup();
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
                callbacks.onCreateActor(name, m_creationParentGuid);
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
    if (m_actorToDeleteGuid.IsValid())
    {
        ImGui::OpenPopup("Confirm Delete Actor");
    }


    // This ia also a modal popup
    if (ImGui::BeginPopupModal("Confirm Delete Actor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
		Actor* actorToDelete = scene
			? scene->ResolveActor(m_actorToDeleteGuid)
			: nullptr;

        ImGui::Text("Do you want to delete the following actor?");
        ImGui::Text("%s", actorToDelete ? actorToDelete->GetName().c_str() : "Unknown");
        ImGui::Separator();

		// Delete button triggers the callback to delete the actor
        if (ImGui::Button("Delete", ImVec2(120, 0)))
        {
            bool deleted = false;

            if (actorToDelete && callbacks.onDeleteActor)
            {
				deleted = callbacks.onDeleteActor(m_actorToDeleteGuid);
            }

            if (deleted && m_selectedActorGuid == m_actorToDeleteGuid)
            {
				m_selectedActorGuid = {};
            }

			m_actorToDeleteGuid = {};
            ImGui::CloseCurrentPopup();
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        // Cancel button just closes the popup without doing anything
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
			m_actorToDeleteGuid = {};
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void HierarchyPanel::RenderActorNode(
    Actor* actor,
    SceneBase* scene,
    const Callbacks& callbacks
)
{
    if (!actor) return;

    auto children = actor->GetDirectChildren();
    bool hasChildren = !children.empty();

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;

    if (m_selectedActorGuid == actor->GetGuid()) flags |= ImGuiTreeNodeFlags_Selected;

    bool opened = ImGui::TreeNodeEx(
        (void*)actor,
        flags,
        "%s",
        actor->GetName().c_str()
    );

    // Left-click to select
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
		m_selectedActorGuid = actor->GetGuid();
    }

    // Begin dragging this Actor.
    HandleActorDragSource(actor);

    // Dropping another Actor onto this node makes this Actor its parent.
    HandleActorDropTarget(
		actor->GetGuid(),
        callbacks
    );

	// Right-click to open the context menu for this actor
    if (ImGui::BeginPopupContextItem())
    {
		m_selectedActorGuid = actor->GetGuid();

		if (ImGui::MenuItem("Rename Actor"))
		{
			m_renameTargetGuid = actor->GetGuid();
			m_showRenamePopup = true;
			std::snprintf(
				m_renameBuffer,
				sizeof(m_renameBuffer),
				"%s",
				actor->GetName().c_str()
			);
		}

        if (ImGui::MenuItem("Delete Actor"))
        {
			m_actorToDeleteGuid = actor->GetGuid();
        }

		if (ImGui::MenuItem("Create Child Actor"))
		{
            m_creationParentGuid = actor->GetGuid();
            m_showActorCreationPopup = true;
            m_newActorNameBuffer[0] = '\0';
		}

        // ==========================================================
		// Additional context menu items can be added here.
        // ==========================================================


        ImGui::EndPopup();
    }
    if (opened)
    {
        for (auto* child : children)
        {
            RenderActorNode(
                child,
                scene,
                callbacks
            );
        }

        ImGui::TreePop();
    }
}

void HierarchyPanel::RenderRootDropTarget(
    const Callbacks& callbacks
)
{
    const bool selected = false;

	// Render a selectable item for the root of the hierarchy
    if (ImGui::Selectable(
        "Scene Root",
        selected,
        ImGuiSelectableFlags_SpanAllColumns))
    {
		m_selectedActorGuid = {};
    }

	// Handle dropping an Actor onto the root of the hierarchy, which makes it a root Actor.
	HandleActorDropTarget({}, callbacks);

    ImGui::Separator();
}

void HierarchyPanel::HandleActorDragSource(Actor* actor)
{
    if (!actor || !actor->GetGuid().IsValid()) return;

	// Begin dragging this Actor.
    // The payload will contain the Actor's Guid,
    // which can be used to identify it when dropped onto another node.
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
		// Get the Guid of the Actor being dragged
        const Guid actorGuid = actor->GetGuid();

		// Set the payload for the drag-and-drop operation.
        ImGui::SetDragDropPayload(
            kActorPayloadType,
            &actorGuid,
            sizeof(Guid)
        );
        
        // Display the name of the Actor being moved
        ImGui::Text(
            "Move %s",
            actor->GetName().c_str()
        );

        ImGui::EndDragDropSource();
    }
}

void HierarchyPanel::HandleActorDropTarget(
	const Guid& newParentGuid,
    const Callbacks& callbacks
)
{
	if (!callbacks.onReparentActor) return;

	// Check if the current item is a valid drop target for drag-and-drop operations.
    if (!ImGui::BeginDragDropTarget()) return;

	// Accept the payload if it matches the expected type for actors.
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kActorPayloadType);

	// If the payload is valid and contains the expected data size,
    // resolve the source Actor by its Guid.
    if (payload && payload->DataSize == sizeof(Guid))
    {
		// Get the Guid of the Actor being dragged from the payload data
        const Guid sourceGuid = *static_cast<const Guid*>(payload->Data);

		callbacks.onReparentActor(sourceGuid, newParentGuid);
    }

    ImGui::EndDragDropTarget();
}
