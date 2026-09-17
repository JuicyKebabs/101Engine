#include "ActorImprintInstanceRegistry.h"
#include "ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/ComponentRegistry.h"

const ActorImprintInstanceRecord* ActorImprintInstanceRegistry::FindInstance(ActorHandle root) const
{
	const auto it = m_instances.find(root);
	return it == m_instances.end() ? nullptr : &it->second;
}

const ActorImprintMembership* ActorImprintInstanceRegistry::FindMember(ActorHandle actor) const
{
	const auto it = m_members.find(actor);
	return it == m_members.end() ? nullptr : &it->second;
}

Actor* ActorImprintInstanceRegistry::ResolveActor(ActorHandle root, LocalObjectId id) const
{
	const auto* record = FindInstance(root);

	if (!record)
	{
		return nullptr;
	}

	const auto it = record->actors.find(id);

	if (it == record->actors.end())
	{
		return nullptr;
	}

	Actor* actor = m_scene.ResolveActor(it->second.handle);
	return actor && actor->GetGuid() == it->second.guid ? actor : nullptr;
}

Component* ActorImprintInstanceRegistry::ResolveComponent(ActorHandle root, LocalObjectId id) const
{
	const auto* record = FindInstance(root);

	if (!record)
	{
		return nullptr;
	}

	const auto it = record->components.find(id);

	if (it == record->components.end())
	{
		return nullptr;
	}

	Actor* actor = ResolveActor(root, it->second.actorId);

	if (!actor || actor->IsDestroyed())
	{
		return nullptr;
	}

	const auto type = ComponentRegistry::Get().GetTypeId(it->second.typeName);
	return type ? actor->GetComponentByExactType(*type, it->second.occurrence) : nullptr;
}

LocalObjectId ActorImprintInstanceRegistry::FindComponentId(ActorHandle root, const Component* component) const
{
	const auto* record = FindInstance(root);

	if (!record || !component)
	{
		return InvalidLocalObjectId;
	}

	for (const auto& [id, locator] : record->components)
	{
		if (ResolveComponent(root, id) == component)
		{
			return id;
		}
	}

	return InvalidLocalObjectId;
}

void ActorImprintInstanceRegistry::OnActorsCollected(const std::vector<ActorHandle>& handles)
{
	for (const auto handle : handles)
	{
		const auto member = m_members.find(handle);

		if (member == m_members.end())
		{
			continue;
		}

		const auto instance = m_instances.find(member->second.root);

		if (instance != m_instances.end())
		{
			instance->second.actors.erase(member->second.objectId);

			if (instance->second.actors.empty())
			{
				m_system->ReleaseInstance(instance->second.imprint);
				m_instances.erase(instance);
			}
		}

		m_members.erase(member);
	}
}
