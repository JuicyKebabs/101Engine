#include "UI/Inspector/Components/TransformInspector.h"
#include "Engine/Component/Transform.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void TransformInspector::Draw(
    Transform& transform,
    const InspectorContext&
)
{
    Vector3 position = transform.GetLocalPosition();
    Vector3 rotation = transform.GetLocalRotationEulerDeg();
    Vector3 scale = transform.GetLocalScale();

	// Property grid
    if (!EditorUI::BeginPropertyGrid("TransformProperties")) return;

    // Draw position
    if (EditorUI::Vector3Field("Position", position, 0.1f))
    {
        transform.SetLocalPosition(position);
    }

	// Draw rotation
    if (EditorUI::Vector3Field("Rotation", rotation, 0.5f))
    {
        transform.SetLocalRotationEulerDeg(rotation);
    }

	// Draw scale
    if (EditorUI::Vector3Field("Scale", scale, 0.1f))
    {
        transform.SetLocalScale(scale);
    }

    EditorUI::EndPropertyGrid();
}
