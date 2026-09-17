#include "ActorImprintEditingObjectMap.h"

#include "Engine/Actor/Actor.h"
#include "Engine/Component/Component.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"

#include <algorithm>
#include <limits>
#include <typeindex>
#include <unordered_set>

namespace
{
	bool IsLive(const Actor* actor, const SceneBase& scene)
	{
		return actor && actor->GetOwner() == &scene && !actor->IsDestroyed() &&
			scene.ResolveActor(actor->GetGuid()) == actor;
	}

	bool IsLive(const Component* component, const SceneBase& scene)
	{
		return component && !component->IsDestroyed() && IsLive(component->GetOwner(), scene);
	}
}

bool ActorImprintEditingObjectMap::Initialize(
	const SceneBase& scene,
	const ActorImprintDefinitionExpansion& expansion,
	LocalObjectId nextLocalObjectId)
{
	if (nextLocalObjectId == InvalidLocalObjectId)
	{
		return false;
	}

	m_actorGuids = expansion.actorGuids;
	m_components = expansion.components;
	m_actorLocalIds.clear();
	m_componentLocalIds.clear();
	m_nextLocalObjectId = nextLocalObjectId;

	for (const auto& [id, guid] : m_actorGuids)
	{
		if (id == InvalidLocalObjectId || id >= m_nextLocalObjectId || !guid.IsValid() ||
			!IsLive(scene.ResolveActor(guid), scene) || !m_actorLocalIds.emplace(guid, id).second)
		{
			return false;
		}
	}

	for (const auto& [id, component] : m_components)
	{
		if (id == InvalidLocalObjectId || id >= m_nextLocalObjectId || !IsLive(component, scene) ||
			!m_componentLocalIds.emplace(component, id).second)
		{
			return false;
		}
	}

	Snapshot validation;
	return CaptureSnapshot(scene, validation);
}

bool ActorImprintEditingObjectMap::Issue(LocalObjectId& outId)
{
	if (m_nextLocalObjectId == InvalidLocalObjectId ||
		m_nextLocalObjectId == (std::numeric_limits<LocalObjectId>::max)())
	{
		return false;
	}

	outId = m_nextLocalObjectId++;
	return true;
}

bool ActorImprintEditingObjectMap::Reconcile(const SceneBase& scene)
{
	auto actorGuids = m_actorGuids;
	auto actorLocalIds = m_actorLocalIds;
	auto components = m_components;
	auto componentLocalIds = m_componentLocalIds;

	for (auto it = actorLocalIds.begin(); it != actorLocalIds.end();)
	{
		if (IsLive(scene.ResolveActor(it->first), scene))
		{
			++it;
			continue;
		}

		actorGuids.erase(it->second);
		it = actorLocalIds.erase(it);
	}

	for (auto it = componentLocalIds.begin(); it != componentLocalIds.end();)
	{
		if (IsLive(it->first, scene))
		{
			++it;
			continue;
		}

		components.erase(it->second);
		it = componentLocalIds.erase(it);
	}

	for (Actor* actor : scene.GetAllActors())
	{
		if (!IsLive(actor, scene))
		{
			continue;
		}

		if (!actorLocalIds.contains(actor->GetGuid()))
		{
			LocalObjectId id;

			if (!Issue(id) || !actorGuids.emplace(id, actor->GetGuid()).second ||
				!actorLocalIds.emplace(actor->GetGuid(), id).second)
			{
				return false;
			}
		}

		for (Component* component : actor->GetAllComponents())
		{
			if (!IsLive(component, scene) || componentLocalIds.contains(component))
			{
				continue;
			}

			LocalObjectId id;

			if (!Issue(id) || !components.emplace(id, component).second ||
				!componentLocalIds.emplace(component, id).second)
			{
				return false;
			}
		}
	}

	m_actorGuids.swap(actorGuids);
	m_actorLocalIds.swap(actorLocalIds);
	m_components.swap(components);
	m_componentLocalIds.swap(componentLocalIds);
	Snapshot validation;
	return CaptureSnapshot(scene, validation);
}

bool ActorImprintEditingObjectMap::CaptureSnapshot(const SceneBase& scene, Snapshot& outSnapshot) const
{
	Snapshot candidate;
	candidate.nextLocalObjectId = m_nextLocalObjectId;

	if (candidate.nextLocalObjectId == InvalidLocalObjectId)
	{
		return false;
	}

	std::unordered_set<Guid> liveActors;
	std::unordered_set<const Component*> liveComponents;

	for (Actor* actor : scene.GetAllActors())
	{
		if (!IsLive(actor, scene))
		{
			continue;
		}

		liveActors.insert(actor->GetGuid());
		const LocalObjectId actorId = FindActor(actor->GetGuid());

		if (actorId == InvalidLocalObjectId || actorId >= m_nextLocalObjectId)
		{
			return false;
		}

		candidate.actors.push_back({ actorId, actor->GetGuid() });

		std::unordered_map<std::type_index, std::size_t> occurrences;

		for (Component* component : actor->GetAllComponents())
		{
			if (!IsLive(component, scene))
			{
				continue;
			}

			liveComponents.insert(component);
			const LocalObjectId componentId = FindComponent(component);
			const std::type_index type = typeid(*component);
			const std::string typeName = ComponentRegistry::Get().GetNameByTypeIndex(type);

			if (componentId == InvalidLocalObjectId || componentId >= m_nextLocalObjectId || typeName.empty())
			{
				return false;
			}

			candidate.components.push_back({ componentId, actor->GetGuid(), typeName, occurrences[type]++ });
		}
	}

	if (liveActors.size() != m_actorGuids.size() || liveComponents.size() != m_components.size())
	{
		return false;
	}

	std::sort(candidate.actors.begin(), candidate.actors.end(), [](const auto& a, const auto& b)
	{
		return a.id < b.id;
	});
	std::sort(candidate.components.begin(), candidate.components.end(), [](const auto& a, const auto& b)
	{
		return a.id < b.id;
	});
	outSnapshot = std::move(candidate);
	return true;
}

bool ActorImprintEditingObjectMap::RestoreSnapshot(const SceneBase& scene, const Snapshot& snapshot)
{
	if (snapshot.nextLocalObjectId == InvalidLocalObjectId)
	{
		return false;
	}

	decltype(m_actorGuids) actorGuids;
	decltype(m_actorLocalIds) actorLocalIds;
	decltype(m_components) components;
	decltype(m_componentLocalIds) componentLocalIds;

	for (const ActorEntry& entry : snapshot.actors)
	{
		Actor* actor = scene.ResolveActor(entry.guid);

		if (entry.id == InvalidLocalObjectId || entry.id >= snapshot.nextLocalObjectId || !IsLive(actor, scene) ||
			!actorGuids.emplace(entry.id, entry.guid).second || !actorLocalIds.emplace(entry.guid, entry.id).second)
		{
			return false;
		}
	}

	for (const ComponentEntry& entry : snapshot.components)
	{
		Actor* actor = scene.ResolveActor(entry.actorGuid);
		const auto type = ComponentRegistry::Get().GetTypeId(entry.typeName);
		Component* component = actor && type ? actor->GetComponentByExactType(*type, entry.occurrenceIndex) : nullptr;

		if (entry.id == InvalidLocalObjectId || entry.id >= snapshot.nextLocalObjectId || !IsLive(component, scene) ||
			!components.emplace(entry.id, component).second || !componentLocalIds.emplace(component, entry.id).second)
		{
			return false;
		}
	}

	std::size_t liveActorCount = 0, liveComponentCount = 0;

	for (Actor* actor : scene.GetAllActors())
	{
		if (!IsLive(actor, scene))
		{
			continue;
		}

		++liveActorCount;

		for (Component* component : actor->GetAllComponents())
		{
			if (IsLive(component, scene))
			{
				++liveComponentCount;
			}
		}
	}

	if (liveActorCount != actorGuids.size() || liveComponentCount != components.size())
	{
		return false;
	}

	m_actorGuids.swap(actorGuids);
	m_actorLocalIds.swap(actorLocalIds);
	m_components.swap(components);
	m_componentLocalIds.swap(componentLocalIds);
	m_nextLocalObjectId = (std::max)(m_nextLocalObjectId, snapshot.nextLocalObjectId);
	return true;
}

LocalObjectId ActorImprintEditingObjectMap::FindActor(const Guid& guid) const
{
	const auto it = m_actorLocalIds.find(guid);
	return it == m_actorLocalIds.end() ? InvalidLocalObjectId : it->second;
}

Guid ActorImprintEditingObjectMap::FindActor(LocalObjectId id) const
{
	const auto it = m_actorGuids.find(id);
	return it == m_actorGuids.end() ? Guid{} : it->second;
}

LocalObjectId ActorImprintEditingObjectMap::FindComponent(const Component* component) const
{
	const auto it = m_componentLocalIds.find(component);
	return it == m_componentLocalIds.end() ? InvalidLocalObjectId : it->second;
}

Component* ActorImprintEditingObjectMap::FindComponent(LocalObjectId id) const
{
	const auto it = m_components.find(id);
	return it == m_components.end() ? nullptr : it->second;
}
