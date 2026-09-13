#include "Component.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Component/ComponentReflection.h"

void Component::MarkForDestruction(StructuralMutationResult* result)
{
	if (m_pOwner && m_pOwner->GetOwner())
	{
		m_pOwner->GetOwner()->RemoveActorComponent(m_pOwner, this, result);
		return;
	}
	m_destroyed = true;
	StructuralMutationResult{}.Report(result);
}

EngineContext* Component::GetEngineContext() const
{
	if (m_pOwner)
	{
		SceneBase* ownerScene = m_pOwner->GetOwner();
		if (ownerScene)
		{
			return ownerScene->GetEngineContext();
		}
	}
	return nullptr;
}

bool Component::Serialize(nlohmann::json& outJson) const
{
	return SerializeReflectedComponent(*this, outJson);
}

bool Component::Deserialize(const nlohmann::json& json)
{
	return DeserializeReflectedComponent(*this, json);
}

bool Component::ResolveReferences(SceneBase& scene)
{
	(void)scene; // Currently, there are no references to resolve in the base Component class.
	return true;
}
