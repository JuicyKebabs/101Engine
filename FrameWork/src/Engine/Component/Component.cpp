#include "Component.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Component/ComponentReflection.h"

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
