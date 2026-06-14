#include "InspectorPanel.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Component/Transform.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Core/Debug/Debug.h"
#include "imgui.h"
#include <typeindex>

void InspectorPanel::Render(Actor* selectedActor)
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

    // Transform
    auto* transform = selectedActor->GetComponentByClass<Transform>();
    if (transform)
    {
        Vector3 pos   = transform->GetLocalPosition();
        Vector3 rot   = transform->GetLocalRotationEulerDeg();
        Vector3 scale = transform->GetLocalScale();

        bool changed = false;
        changed |= ImGui::DragFloat3("Position", &pos.x, 0.1f);
        changed |= ImGui::DragFloat3("Rotation", &rot.x, 0.5f);
        changed |= ImGui::DragFloat3("Scale",    &scale.x, 0.1f);

        if (changed)
        {
            transform->SetLocalPosition(pos);
            transform->SetLocalRotationEulerDeg(rot);
            transform->SetLocalScale(scale);
        }
    }

    ImGui::Separator();

    // List currently attached components
    ImGui::Text("Components:");
    for (auto& typeId : selectedActor->GetComponentsTypeIds())
    {
        if (typeId == std::type_index(typeid(Transform))) continue;

        std::string name = ComponentRegistry::Get().GetNameByTypeIndex(typeId);
        if (!name.empty())
        {
            ImGui::BulletText("%s", name.c_str());
        }
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
