#include "UI/Inspector/Components/TransformInspector.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void TransformInspector::Draw(Transform& transform, const InspectorContext& context)
{
	Actor* owner = transform.GetOwner();
	if (!owner) return;

    const Guid ownerGuid = owner->GetGuid();
	if (!ownerGuid.IsValid()) return;

    Vector3 position = transform.GetLocalPosition();
    Vector3 rotation = transform.GetLocalRotationEulerDeg();
    Vector3 scale = transform.GetLocalScale();

	// Property grid
	if (!EditorUI::BeginPropertyGrid("TransformProperties")) return;

    // Save the transform state before editing, to support undo/redo
    const Transform3D beforeField = transform.GetLocalTransform();

    //----------------
    // Draw position
	//----------------

	// Draw the position field and get the result of the edit operation
    const EditorUI::EditResult result = EditorUI::Vector3Field("Position", position, 0.1f);

	// If the user starts editing the position, notify the context
	if (result.activated && context.onTransformEditBegin)
	{
		context.onTransformEditBegin(ownerGuid, beforeField);
	}

	// If the position was changed, update the transform's local position
    if (result.changed) transform.SetLocalPosition(position);

	// If the user finished editing the position, notify the context (stuck command for undo/redo)
	if (result.deactivatedAfterEdit && context.onTransformEditEnd)
	{
		context.onTransformEditEnd(ownerGuid, transform.GetLocalTransform());
	}

	//-----------------
	// Draw rotation
	//-----------------
	const EditorUI::EditResult rotationResult = EditorUI::Vector3Field("Rotation", rotation, 0.5f);
	if (rotationResult.activated && context.onTransformEditBegin) context.onTransformEditBegin(ownerGuid, beforeField);
	if (rotationResult.changed) transform.SetLocalRotationEulerDeg(rotation);
	if (rotationResult.deactivatedAfterEdit && context.onTransformEditEnd) context.onTransformEditEnd(ownerGuid, transform.GetLocalTransform());

	//-----------------
	// Draw scale
	//-----------------
	const EditorUI::EditResult scaleResult = EditorUI::Vector3Field("Scale", scale, 0.1f);
	if (scaleResult.activated && context.onTransformEditBegin) context.onTransformEditBegin(ownerGuid, beforeField);
	if (scaleResult.changed) transform.SetLocalScale(scale);
	if (scaleResult.deactivatedAfterEdit && context.onTransformEditEnd) context.onTransformEditEnd(ownerGuid, transform.GetLocalTransform());

    EditorUI::EndPropertyGrid();
}
