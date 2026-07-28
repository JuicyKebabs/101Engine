#include "InspectorPanel.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Core/Debug/Debug.h"
#include "imgui.h"
#include <typeindex>

void InspectorPanel::Render(Actor* selectedActor, const InspectorContext& context)
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

    bool isActive = selectedActor->IsActive();
    if (ImGui::Checkbox("Active", &isActive))
    {
        selectedActor->SetActive(isActive);
    }

    ImGui::Separator();

	// Draw each component using the registered drawer functions
    for (auto& component : selectedActor->GetAllComponents())
    {
		if (!component || component->IsDestroyed()) continue;

		// Draw the component using the registered drawer function
        DrawComponent(*component, context);
    }

    ImGui::Separator();

    // Attach a component by typing its registered name
    ImGui::InputText("##ComponentName", m_componentNameBuffer, sizeof(m_componentNameBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Add Component"))
    {
        std::string name = m_componentNameBuffer;
        if (!name.empty())
        {
            if (ComponentRegistry::Get().AddToActor(name, selectedActor))
            {
                DBG("InspectorPanel: Added component '%s'", name.c_str());
            }
            else
            {
                DBG("InspectorPanel: Unknown component type '%s'", name.c_str());
            }
            m_componentNameBuffer[0] = '\0';
        }
    }

    ImGui::End();
}

void InspectorPanel::DrawComponent(
    Component& component,
    const InspectorContext& context
)
{
	const std::type_index typeId = std::type_index(typeid(component));

	std::string componentName = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

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
        ImGuiTreeNodeFlags_DefaultOpen
    );

    if (opened)
    {
		// Draw the component's inspector UI using the registered drawer function
		const bool drawn = m_componentInspectorRegistry.Draw(component, context);

		if (!drawn)
		{
			ImGui::TextDisabled("No inspector is registered for this component.");
		}
    }

	ImGui::PopID(); // Pop the unique ID for ImGui
}
