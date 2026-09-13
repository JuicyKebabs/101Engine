#pragma once
#include "ActorImprintHandle.h"
#include "DefinitionRevision.h"
#include "LocalObjectId.h"
#include "Engine/Actor/ActorHandle.h"
#include <string>
#include <unordered_map>
#include <vector>

class Actor;
class Component;
class SceneBase;
class ActorImprintSystem;

struct ActorImprintActorIdentity
{
	Guid guid;
	ActorHandle handle;
};

struct ActorImprintComponentLocator
{
	LocalObjectId actorId = InvalidLocalObjectId;
	std::string typeName;
	std::size_t occurrence = 0;
};

struct ActorImprintInstanceRecord
{
	Guid assetGuid;
	ActorImprintHandle imprint;
	DefinitionRevision sourceRevision;
	LocalObjectId rootId = InvalidLocalObjectId;
	ActorHandle root;
	bool destroying = false;
	std::unordered_map<LocalObjectId, ActorImprintActorIdentity> actors;
	std::unordered_map<LocalObjectId, ActorImprintComponentLocator> components;
};

struct ActorImprintMembership
{
	ActorHandle root;
	LocalObjectId objectId = InvalidLocalObjectId;
};

// Scene-owned provenance and derived lookup indices. Actors remain owned by ActorPool.
class ActorImprintInstanceRegistry
{
public:
	explicit ActorImprintInstanceRegistry(const SceneBase& scene) : m_scene(scene) {}
	ActorImprintInstanceRegistry(const ActorImprintInstanceRegistry&) = delete;
	ActorImprintInstanceRegistry& operator=(const ActorImprintInstanceRegistry&) = delete;
	const ActorImprintInstanceRecord* FindInstance(ActorHandle root) const;
	const ActorImprintMembership* FindMember(ActorHandle actor) const;
	Actor* ResolveActor(ActorHandle root, LocalObjectId id) const;
	Component* ResolveComponent(ActorHandle root, LocalObjectId id) const;
	LocalObjectId FindComponentId(ActorHandle root, const Component* component) const;
	const auto& GetInstances() const { return m_instances; }

private:
	friend class ActorImprintSystem;
	friend class SceneBase;
	void OnActorsCollected(const std::vector<ActorHandle>& handles);
	const SceneBase& m_scene;
	ActorImprintSystem* m_system = nullptr; // App outlives Scene; pins definitions for registered records.
	std::unordered_map<ActorHandle, ActorImprintInstanceRecord> m_instances;
	std::unordered_map<ActorHandle, ActorImprintMembership> m_members;
};
