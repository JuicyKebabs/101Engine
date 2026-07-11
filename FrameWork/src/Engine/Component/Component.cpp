#include "Component.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Context/Context.h"

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