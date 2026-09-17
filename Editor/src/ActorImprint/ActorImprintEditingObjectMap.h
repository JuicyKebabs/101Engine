#pragma once

#include "Engine/ActorImprint/ActorImprintDefinitionExpander.h"
#include "Engine/ActorImprint/LocalObjectId.h"
#include "Engine/Core/GUID/Guid.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class Component;
class SceneBase;

class ActorImprintEditingObjectMap
{
public:
	struct ActorEntry
	{
		LocalObjectId id = InvalidLocalObjectId;
		Guid guid;
	};

	struct ComponentEntry
	{
		LocalObjectId id = InvalidLocalObjectId;
		Guid actorGuid;
		std::string typeName;
		std::size_t occurrenceIndex = 0;
	};

	struct Snapshot
	{
		std::vector<ActorEntry> actors;
		std::vector<ComponentEntry> components;
		LocalObjectId nextLocalObjectId = InvalidLocalObjectId;
	};

	bool Initialize(
		const SceneBase& scene,
		const ActorImprintDefinitionExpansion& expansion,
		LocalObjectId nextLocalObjectId);
	bool Reconcile(const SceneBase& scene);
	bool CaptureSnapshot(const SceneBase& scene, Snapshot& outSnapshot) const;
	bool RestoreSnapshot(const SceneBase& scene, const Snapshot& snapshot);

	LocalObjectId FindActor(const Guid& guid) const;
	Guid FindActor(LocalObjectId id) const;
	LocalObjectId FindComponent(const Component* component) const;
	Component* FindComponent(LocalObjectId id) const;
	LocalObjectId GetNextLocalObjectId() const { return m_nextLocalObjectId; }
	std::size_t GetActorCount() const { return m_actorGuids.size(); }
	std::size_t GetComponentCount() const { return m_components.size(); }

private:
	bool Issue(LocalObjectId& outId);

	std::unordered_map<LocalObjectId, Guid> m_actorGuids;
	std::unordered_map<Guid, LocalObjectId> m_actorLocalIds;
	std::unordered_map<LocalObjectId, Component*> m_components;
	std::unordered_map<const Component*, LocalObjectId> m_componentLocalIds;
	LocalObjectId m_nextLocalObjectId = InvalidLocalObjectId;
};
