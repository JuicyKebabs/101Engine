#include "HierarchyPanel.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Actor/Actor.h"
#include "Engine/UI/Canvas.h"
#include "UI/AssetDragDropPayload.h"
#include "imgui.h"
#include <cstdio>

void HierarchyPanel::Render(
	SceneBase* scene,
	EditorSelection& selection,
	const Callbacks& callbacks)
{
    const bool isOpen = ImGui::Begin("Hierarchy");

    if (isOpen)
    {
        if (scene)
        {
            // Always provide an explicit destination for moving an Actor
            // out of its current parent hierarchy.
			RenderRootDropTarget(scene, selection, callbacks);

            for (auto* actor : scene->GetRootActors())
            {
				RenderActorNode(actor, scene, selection, callbacks);
			}
		}

		if (!m_diagnostic.empty())
		{
			ImGui::Separator();
			ImGui::TextWrapped("%s", m_diagnostic.c_str());
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
				ChangeSelection(selection, {}, callbacks);
            }
        }
    }

    ImGui::End();

    // Menu popup
    if (m_showMenuPopup)
    {
        ImGui::OpenPopup("HierarchyContextMenu");
        m_showMenuPopup = false;
    }

    if (ImGui::BeginPopup("HierarchyContextMenu"))
    {
        // Menu item for creating a new actor
        if (ImGui::MenuItem("Create Empty Actor", nullptr, false, callbacks.canEdit))
        {
			m_creationParentGuid = ResolveEmptySpaceCreationParent(scene);
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

			if (renamed)
			{
				ImGui::CloseCurrentPopup();
			}
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

		if (!m_diagnostic.empty())
		{
			ImGui::TextWrapped("%s", m_diagnostic.c_str());
		}

		ImGui::Separator();

        // Create button triggers the callback to create the script
        if (ImGui::Button("Create", ImVec2(120, 0)))
		{
			std::string name = m_newActorNameBuffer;

			if (!name.empty() && callbacks.onCreateActor &&
				callbacks.onCreateActor(name, m_creationParentGuid))
			{
				m_diagnostic.clear();
				ImGui::CloseCurrentPopup();
			}
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
				const bool deletesSelection = selection.GetSelectedActorGuid() == m_actorToDeleteGuid;

				if (deletesSelection && callbacks.onSelectionChanging)
				{
					callbacks.onSelectionChanging();
				}

				deleted = callbacks.onDeleteActor(m_actorToDeleteGuid);
			}

			if (deleted && selection.GetSelectedActorGuid() == m_actorToDeleteGuid)
			{
				selection.Clear();
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
	EditorSelection& selection,
	const Callbacks& callbacks)
{
	if (!actor)
	{
		return;
	}

	auto children = actor->GetDirectChildren();
    bool hasChildren = !children.empty();

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;

	if (!hasChildren)
	{
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	if (selection.GetSelectedActorGuid() == actor->GetGuid())
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool opened = ImGui::TreeNodeEx((void*)actor, flags, "%s", actor->GetName().c_str());

    // Left-click to select
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
		ChangeSelection(selection, actor->GetGuid(), callbacks);

		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
			actor->GetComponentByClass<Canvas>() &&
			callbacks.onOpenCanvas)
		{
			callbacks.onOpenCanvas(actor->GetGuid());
		}
    }

    // Begin dragging this Actor.
    HandleActorDragSource(actor, callbacks);

    // Dropping another Actor onto this node makes this Actor its parent.
	HandleDropTarget(scene, actor->GetGuid(), callbacks);

	// Right-click to open the context menu for this actor
    if (ImGui::BeginPopupContextItem())
    {
		ChangeSelection(selection, actor->GetGuid(), callbacks);

		if (ImGui::MenuItem("Rename Actor", nullptr, false, callbacks.canEdit))
		{
			m_renameTargetGuid = actor->GetGuid();
			m_showRenamePopup = true;
			std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s", actor->GetName().c_str());
		}

		const bool canDestroy = scene && scene->CanDestroy(actor, true);

		if (ImGui::MenuItem("Delete Actor", nullptr, false,
			callbacks.canEdit && canDestroy))
        {
			m_actorToDeleteGuid = actor->GetGuid();
		}

		if (callbacks.canEdit && !canDestroy)
		{
			ImGui::TextDisabled("Required root Actor cannot be deleted.");
		}

		if (ImGui::MenuItem("Create Child Actor", nullptr, false, callbacks.canEdit))
		{
            m_creationParentGuid = actor->GetGuid();
            m_showActorCreationPopup = true;
            m_newActorNameBuffer[0] = '\0';
		}

		if (actor->GetComponentByClass<Canvas>() &&
			ImGui::MenuItem("Open in Canvas View"))
		{
			if (callbacks.onOpenCanvas)
			{
				callbacks.onOpenCanvas(actor->GetGuid());
			}
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
			RenderActorNode(child, scene, selection, callbacks);
        }

        ImGui::TreePop();
    }
}

void HierarchyPanel::RenderRootDropTarget(
	SceneBase* scene,
	EditorSelection& selection,
	const Callbacks& callbacks)
{
    const bool selected = false;

	// Render a selectable item for the root of the hierarchy
    if (ImGui::Selectable("Scene Root", selected, ImGuiSelectableFlags_SpanAllColumns))
    {
		ChangeSelection(selection, {}, callbacks);
    }

	// Handle dropping an Actor onto the root of the hierarchy, which makes it a root Actor.
	HandleDropTarget(scene, {}, callbacks);

    ImGui::Separator();
}

void HierarchyPanel::ChangeSelection(
	EditorSelection& selection,
	const Guid& actorGuid,
	const Callbacks& callbacks)
{
	if (selection.GetSelectedActorGuid() == actorGuid)
	{
		return;
	}

	if (callbacks.onSelectionChanging)
	{
		callbacks.onSelectionChanging();
	}

	selection.SelectActor(actorGuid);
}

Guid HierarchyPanel::ResolveEmptySpaceCreationParent(const SceneBase* scene)
{
	if (!scene || scene->GetStructurePolicy() != SceneStructurePolicy::SingleRootClosedSubtree)
	{
		return {};
	}

	const auto roots = scene->GetRootActors();

	if (roots.size() != 1 || !roots.front() || roots.front()->IsDestroyed())
	{
		return {};
	}

	return roots.front()->GetGuid();
}

void HierarchyPanel::HandleActorDragSource(Actor* actor, const Callbacks& callbacks)
{
	if (!actor || !actor->GetGuid().IsValid() || !callbacks.onReparentActor || !callbacks.canEdit)
	{
		return;
	}

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

void HierarchyPanel::HandleDropTarget(
	SceneBase* scene,
	const Guid& newParentGuid,
	const Callbacks& callbacks)
{
	if ((!callbacks.onReparentActor && !callbacks.onInstantiateActorImprint) || !callbacks.canEdit)
	{
		return;
	}

	// Check if the current item is a valid drop target for drag-and-drop operations.
	if (!ImGui::BeginDragDropTarget())
	{
		return;
	}

	// Peek before accepting so invalid hierarchy targets never consume the drop.
	bool canAcceptActor = callbacks.onReparentActor != nullptr;

	if (const ImGuiPayload* incoming = ImGui::GetDragDropPayload();
		canAcceptActor && incoming && incoming->IsDataType(kActorPayloadType) &&
		incoming->DataSize == sizeof(Guid))
	{
		const Guid sourceGuid = *static_cast<const Guid*>(incoming->Data);
		Actor* source = scene ? scene->ResolveActor(sourceGuid) : nullptr;
		Actor* parent = scene && newParentGuid.IsValid() ? scene->ResolveActor(newParentGuid) : nullptr;
		canAcceptActor = scene && static_cast<bool>(scene->CanReparent(source, parent));

		if (!canAcceptActor)
		{
			ImGui::SetTooltip("This hierarchy target is not valid for the dragged Actor.");
		}
	}

	const ImGuiPayload* payload = canAcceptActor
		? ImGui::AcceptDragDropPayload(kActorPayloadType) : nullptr;

	// If the payload is valid and contains the expected data size,
    // resolve the source Actor by its Guid.
    if (payload && payload->DataSize == sizeof(Guid))
    {
		// Get the Guid of the Actor being dragged from the payload data
        const Guid sourceGuid = *static_cast<const Guid*>(payload->Data);

		callbacks.onReparentActor(sourceGuid, newParentGuid);
    }

	const ImGuiPayload* assetPayload =
		callbacks.onInstantiateActorImprint ? ImGui::AcceptDragDropPayload(EditorAssetDragDropPayloadType) : nullptr;

	if (assetPayload && assetPayload->DataSize == sizeof(EditorAssetDragDropPayload))
	{
		const auto& asset = *static_cast<const EditorAssetDragDropPayload*>(assetPayload->Data);

		if (asset.assetType == AssetType::ActorImprint && asset.assetGuid.IsValid())
		{
			callbacks.onInstantiateActorImprint(asset.assetGuid, newParentGuid);
		}
	}

	ImGui::EndDragDropTarget();
}
