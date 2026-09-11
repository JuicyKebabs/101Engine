#include "InspectorPanel.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Scene/SceneBase.h"
#include "UI/Inspector/AssetPicker.h"
#include "UI/EditorUI.h"
#include "imgui.h"
#include <typeindex>
#include <unordered_map>
#include <vector>

void InspectorPanel::Render(Actor* selectedActor, const InspectorContext& context, const Callbacks& callbacks)
{
    ImGui::Begin("Inspector");

    if (!selectedActor)
    {
        ImGui::Text("No actor selected.");
        ImGui::End();
        return;
    }

    // Basic info
    ImGui::Text("Name: %s", selectedActor->GetName().c_str());
    ImGui::Text("Tag: %s", TagRegistry::Get().GetName(selectedActor->GetTag()).c_str());

	bool readOnly = (context.state == InspectorState::ReadOnly);

    {
        EditorUI::DisabledScope disabledScope(readOnly);
        bool isActive = selectedActor->IsActive();
        if (ImGui::Checkbox("Active", &isActive))
        {
            selectedActor->SetActive(isActive);
        }
    }

    ImGui::Separator();

    std::unordered_map<std::type_index, std::size_t> occurrenceCounts;

    ComponentRemovalRequest removalRequest;

	// Draw each component using the registered drawer functions
    for (auto& component : selectedActor->GetAllComponents())
    {
		if (!component || component->IsDestroyed()) continue;

        const std::type_index typeId = typeid(*component);
        const std::size_t occurrenceIndex = occurrenceCounts[typeId]++;

		// Draw the component using reflection metadata.
		const bool removeRequested = DrawComponent(
			*component,
			occurrenceIndex,
			selectedActor->GetGuid(),
			context,
			callbacks,
			readOnly);
        if (removeRequested)
		{// If the "Remove" button was clicked, prepare a removal request
            const std::string componentName = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

            if (!componentName.empty())
            {
                removalRequest.componentName = componentName;
                removalRequest.occurrenceIndex = occurrenceIndex;
                removalRequest.requested = true;
            }
        }
    }

	// Handle component removal request after the loop to avoid modifying the collection during iteration
    if (removalRequest.requested && callbacks.onRemoveComponent)
    {
        callbacks.onRemoveComponent(
            selectedActor->GetGuid(),
            removalRequest.componentName,
            removalRequest.occurrenceIndex
        );
    }

    ImGui::Separator();

	// Draw the "Add Component" section

	// Buffer to hold the name of the component to be added
    std::string pendingAddComponent;

    {
        EditorUI::DisabledScope addComponentDisabledScope(readOnly);
        if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f))) ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
		// Get the list of names of all registered components from the ComponentRegistry
        const std::vector<std::string> names = ComponentRegistry::Get().GetRegisteredComponentNames();

        for (const std::string& name : names)
        {
			// Check if the component can be added to the selected actor
            // based on its cardinality and existing components
            const bool canAdd = ComponentRegistry::Get().CanAddToActor(name, selectedActor);

			const bool disabled = readOnly || !canAdd;

			// Disable the following selectable item
            // if the component cannot be added
            ImGui::BeginDisabled(disabled);

			// Draw a selectable item for the component name
            if (ImGui::Selectable(name.c_str()))
            {
                pendingAddComponent = name;
            }

            ImGui::EndDisabled();
        }

        ImGui::EndPopup();
    }

	// If a component was selected to be added, invoke the callback
    if (!pendingAddComponent.empty() && callbacks.onAddComponent)
    {
        callbacks.onAddComponent(selectedActor->GetGuid(), pendingAddComponent);
    }

    ImGui::End();
}

bool InspectorPanel::DrawComponent(
	Component& component,
	std::size_t occurrenceIndex,
	const Guid& actorGuid,
	const InspectorContext& context,
	const Callbacks& callbacks,
	bool readOnly)
{
	const std::type_index typeId = std::type_index(typeid(component));

	// Get the registered name of the component type from the ComponentRegistry
	std::string componentName = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

	// Get the component policy to judge if it can be removed (not unique required)
	const auto policy = ComponentRegistry::Get().GetPolicy(typeId);

	// Flag to indicate if the component can be removed (not unique required)
    const bool removable = policy && policy->cardinality != ComponentCardinality::UniqueRequired;

	if (componentName.empty())
	{
		// If the component name is not registered, 
        // use the component's own name or a default
		componentName = component.GetName();

		if (componentName.empty())
		{
			// If the component has no name, show as "Unnamed Component"
			componentName = "Unnamed Component";
		}
	}

	ImGui::PushID(&component); // Ensure unique ID for ImGui

	// Draw a collapsible header for the component
	const bool opened = ImGui::CollapsingHeader(
        componentName.c_str(), 
        ImGuiTreeNodeFlags_DefaultOpen |
        ImGuiTreeNodeFlags_AllowOverlap
    );

	// Flag to indicate if the component removal was requested
    bool removeRequested = false;

	// Draw a "Remove" button if the component is removable
    if (removable)
    {
        const float buttonWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - buttonWidth);

		// Draw the "Remove" button and set the flag if clicked
		EditorUI::DisabledScope disabledScope(readOnly);
        if (ImGui::SmallButton("Remove")) removeRequested = true;
    }

	// Draw the inspector UI for the component if the header is opened
    if (opened)
    {
		const TypeMetadata* metadata = ComponentRegistry::Get().GetMetadata(typeId);
		if (!metadata)
		{
			ImGui::TextDisabled("Reflection metadata is not registered for this component.");
		}
		else
		{
			ReflectionInspectorPolicy inspectorPolicy = ReflectionInspectorPolicy::Editable;
			if (readOnly)
			{
				inspectorPolicy = ReflectionInspectorPolicy::ReadOnly;
			}
			if (ReflectionInspector::BuildRows(*metadata, inspectorPolicy).empty())
			{
				ImGui::TextDisabled("No inspectable properties.");
			}
			else
			{
				ReflectionInspectorCallbacks reflectionCallbacks;
				SceneBase* editScene = context.scene;
				const Guid editActorGuid = actorGuid;
				const std::size_t editOccurrenceIndex = occurrenceIndex;
				reflectionCallbacks.restoreValue = [
					editScene, editActorGuid, typeId, editOccurrenceIndex](
					const PropertyMetadata& property,
					const PropertyValue& value)
				{
					ComponentPropertyIdentity identity{
						editActorGuid,
						typeId,
						editOccurrenceIndex,
						property.GetPath() };
					return ApplyComponentPropertyValue(editScene, identity, value);
				};
				reflectionCallbacks.onEditCommit = [&, typeId](
					const PropertyMetadata& property,
					const PropertyValue& before,
					const PropertyValue& after)
				{
					if (!callbacks.onEditProperty) return false;
					return callbacks.onEditProperty(
						ComponentPropertyIdentity{ actorGuid, typeId, occurrenceIndex, property.GetPath() },
						before, after);
				};

				ReflectionInspectorServices services;
				services.drawActorReference = [&context](
					const char* label, const PropertyMetadata&, const ActorReference& current, ActorReference& selected)
				{
					if (!context.scene)
					{
						return false;
					}
					Actor* currentActor = current.Resolve(*context.scene);
					const char* preview = "<None>";
					if (current.HasValue())
					{
						preview = "<Missing Actor>";
						if (currentActor)
						{
							preview = currentActor->GetName().c_str();
						}
					}
					if (!ImGui::BeginCombo(label, preview)) return false;
					bool changed = false;
					if (ImGui::Selectable("<None>", !current.HasValue()) && current.HasValue())
					{
						selected.Clear();
						changed = true;
					}
					for (Actor* actor : context.scene->GetAllActors())
					{
						if (!actor)
						{
							continue;
						}
						if (actor->IsDestroyed())
						{
							continue;
						}
						if (actor->GetOwner() != context.scene)
						{
							continue;
						}
						const bool isSelected = current.GetGuid() == actor->GetGuid();
						ImGui::PushID(actor);
						if (ImGui::Selectable(actor->GetName().c_str(), isSelected) && !isSelected)
						{
							selected.Set(actor);
							changed = true;
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
					return changed;
				};
				services.drawAssetReference = [&context](
					const char* label, const PropertyMetadata& property,
					const AssetReferenceValue& current, AssetReferenceValue& selected)
				{
					if (!context.assetManager) return false;
					Guid selectedGuid;
					const bool selectionChanged = AssetPicker::Draw(
						label,
						*context.assetManager,
						property.GetAssetType(),
						current.guid,
						selectedGuid);
					if (!selectionChanged)
						return false;
					selected = { selectedGuid, property.GetAssetType(), false };
					return true;
				};

				m_reflectionInspector.Draw(
					*metadata, typeId, &component, inspectorPolicy,
					reflectionCallbacks, services);
			}
		}
    }

	ImGui::PopID(); // Pop the unique ID for ImGui

	return removeRequested; // Return whether removal was requested
}
