#include "TransformEditCommand.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"

TransformEditCommand::TransformEditCommand(
	SceneBase* scene,
	const Guid& actorGuid,
	const Transform3D& before,
	const Transform3D& after
)
	: m_scene(scene)
	, m_actorGuid(actorGuid)
	, m_before(before)
	, m_after(after)
{}

bool TransformEditCommand::Execute()
{
	return Apply(m_after);
}

bool TransformEditCommand::Undo()
{
	return Apply(m_before);
}

bool TransformEditCommand::Apply(const Transform3D& state)
{
	if (!m_scene || !m_actorGuid.IsValid()) return false;

	Actor* actor = m_scene->ResolveActor(m_actorGuid);

	if (!actor ||
		actor->IsDestroyed() ||
		actor->GetOwner() != m_scene)
	{
		return false;
	}

	Component* component = actor->GetComponentByExactType(
		std::type_index(typeid(Transform)),
		0
	);

	Transform* transform = static_cast<Transform*>(component);
	if (!transform) return false;

	transform->SetLocalTransform(state);
	return true;
}
