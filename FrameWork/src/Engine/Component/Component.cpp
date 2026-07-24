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

bool Component::Serialize(nlohmann::json& outJson) const
{
	outJson = nlohmann::json::object();
	outJson["name"] = m_name;
	return true;
}

bool Component::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object()) return false;

	if (json.contains("name"))
	{
		if (!json["name"].is_string()) return false;

		SetName(json["name"].get<std::string>());
	}

	return true;
}

bool Component::ResolveReferences(SceneBase& scene)
{
	(void)scene; // Currently, there are no references to resolve in the base Component class.
	return true;
}