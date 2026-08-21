#include "RectTransformEditCommand.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"

RectTransformEditCommand::RectTransformEditCommand(
	SceneBase* scene,
	const Guid& actorGuid,
	const RectTransformEditState& before,
	const RectTransformEditState& after
)
	: m_scene(scene)
	, m_actorGuid(actorGuid)
	, m_before(before)
	, m_after(after)
{}

bool RectTransformEditCommand::Execute()
{
	return Apply(m_after);
}

bool RectTransformEditCommand::Undo()
{
	return Apply(m_before);
}

bool RectTransformEditCommand::Apply(const RectTransformEditState& state)
{
	// Validate Scene and Actor GUID
	if (!m_scene || !m_actorGuid.IsValid()) return false;

	// Get Actor
	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	// Validate Actor
	if (!actor || actor->IsDestroyed() || actor->GetOwner() != m_scene) return false;

	// Check if the actor has a RectTransform component
	Component* component = actor->GetComponentByExactType(std::type_index(typeid(RectTransform)), 0);

	RectTransform* rectTransform = static_cast<RectTransform*>(component);
	if (!rectTransform) return false;

	// Apply the state to the RectTransform component
	state.ApplyTo(*rectTransform);

	return true;
}

RectTransformEditState RectTransformEditState::Capture(const RectTransform& rectTransform)
{
	RectTransformEditState state;
	state.anchorMode = rectTransform.GetAnchorMode();
	state.anchoredPosition = rectTransform.GetAnchoredPosition();
	state.pivot = rectTransform.GetPivot();
	state.size = rectTransform.GetSize();
	state.localRotation = rectTransform.GetLocalRotationQuat();
	state.localScale = rectTransform.GetLocalScale();
	return state;
}

void RectTransformEditState::ApplyTo(RectTransform& rectTransform) const
{
	rectTransform.SetAnchorMode(anchorMode);
	rectTransform.SetAnchoredPosition(anchoredPosition);
	rectTransform.SetPivot(pivot);
	rectTransform.SetSizeDelta(size);
	rectTransform.SetLocalRotationQuat(localRotation);
	rectTransform.SetLocalScale(localScale);
}