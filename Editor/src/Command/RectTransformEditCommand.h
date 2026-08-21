#pragma once
#include "IEditorCommand.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Core/Math/Math.h"

class SceneBase;

//--------------------------------------------------------------------------------------------------------
// RectTransformEditCommand class
// This class represents a command to edit the RectTransform of an actor in the scene.
// Stores the state of the RectTransform before and after the edit, allowing for undo/redo functionality.
//--------------------------------------------------------------------------------------------------------

// Struct to hold the state of a RectTransform for editing purposes
struct RectTransformEditState
{
	AnchorMode anchorMode = AnchorMode::MiddleCenter;
	Vector2 anchoredPosition = Vector2::Zero();
	Vector2 pivot = { 0.5f, 0.5f };
	Vector2 size = Vector2::One();
	Quaternion localRotation = Quaternion::Identity();
	Vector3  localScale = Vector3::One();

	static RectTransformEditState Capture(const RectTransform& rectTransform);
	void ApplyTo(RectTransform& rectTransform) const;

	bool operator==(const RectTransformEditState& other) const
	{
		return anchorMode == other.anchorMode &&
			anchoredPosition.NearEqual(other.anchoredPosition) &&
			pivot.NearEqual(other.pivot) &&
			size.NearEqual(other.size) &&
			localRotation.NearEqual(other.localRotation) &&
			localScale.NearEqual(other.localScale);
	}
};

class RectTransformEditCommand : public IEditorCommand
{
public:
	RectTransformEditCommand(
		SceneBase* scene,
		const Guid& actorGuid,
		const RectTransformEditState& before,
		const RectTransformEditState& after
	);

	bool Execute() override;
	bool Undo() override;

private:
	SceneBase* m_scene = nullptr;	// Pointer to the scene for resolving the Actor
	Guid m_actorGuid;				// GUID for the Actor owning the edited RectTransform

	RectTransformEditState m_before;	// State before editing the RectTransform
	RectTransformEditState m_after;		// State after editing the RectTransform

private:
	bool Apply(const RectTransformEditState& state);	// Apply the given RectTransform state to the Actor
};