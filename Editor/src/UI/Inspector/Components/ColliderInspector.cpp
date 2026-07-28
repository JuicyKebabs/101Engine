#include "UI/Inspector/Components/ColliderInspector.h"
#include "Engine/Component/Collider.h"
#include "UI/EditorUI.h"
#include "UI/Inspector/InspectorContext.h"

void ColliderInspector::Draw(Collider& collider, const InspectorContext&)
{
	const Transform3D& localTransform = collider.GetLocalTransform();
	Vector3 center = localTransform.position;
	Vector3 rotation = localTransform.rotation.ToEulerDeg();
	Vector3 scale = localTransform.scale;
	int type = static_cast<int>(collider.GetType());
	int layer = static_cast<int>(collider.GetLayer());
	bool isTrigger = collider.IsTrigger();
	bool isActive = collider.isActive();

	static const char* typeItems[] = {
		"Box",
		"Sphere",
		"Capsule",
		"None"
	};

	static const char* layerItems[] = {
		"Default",
		"Player",
		"Enemy",
		"Wall",
		"Player Bullet",
		"Player Ray",
		"Enemy Bullet"
	};

	if (!EditorUI::BeginPropertyGrid("ColliderProperties")) return;

	if (EditorUI::ComboField("Type", type, typeItems, 4)) collider.SetType(static_cast<ColliderType>(type));
	if (EditorUI::ComboField("Layer", layer, layerItems, 7)) collider.SetLayer(static_cast<CollisionLayer>(layer));
	if (EditorUI::BoolField("Trigger", isTrigger)) collider.SetTrigger(isTrigger);
	if (EditorUI::BoolField("Active", isActive)) collider.SetActive(isActive);
	if (EditorUI::Vector3Field("Center", center)) collider.SetLocalCenter(center);

	if (EditorUI::Vector3Field("Rotation", rotation, 0.5f))
	{
		collider.SetLocalRotation(Quaternion::CreateFromEulerDeg(rotation));
	}

	if (EditorUI::Vector3Field("Scale", scale)) collider.SetLocalScale(scale);

	EditorUI::EndPropertyGrid();
}
