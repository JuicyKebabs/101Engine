#pragma once
#include "DefinitionRevision.h"
#include "LocalObjectId.h"
#include "nlohmann/json.hpp"
#include <typeindex>
#include <utility>
#include <vector>

class ActorImprintAssetDeserializer;

class ActorImprint
{
public:
	struct ComponentDefinition
	{
		LocalObjectId id;
		std::string typeName; // Stable persistence name, distinct from the resolved runtime type.
		std::type_index type;
		nlohmann::json properties;
	};

	struct ActorDefinition
	{
		LocalObjectId id;
		LocalObjectId parentId; // Zero only for root; JSON uses null.
		nlohmann::json properties;
		std::vector<ComponentDefinition> components;
	};

	static constexpr std::uint32_t SCHEMA_VERSION = 1;
	const DefinitionRevision& GetRevision() const { return m_revision; }
	LocalObjectId GetRootActorId() const { return m_rootActorId; }
	LocalObjectId GetNextLocalObjectId() const { return m_nextLocalObjectId; }
	const std::vector<ActorDefinition>& GetActors() const { return m_actors; }

private:
	friend class ActorImprintAssetDeserializer;
	ActorImprint(
		DefinitionRevision revision,
		LocalObjectId rootId,
		LocalObjectId nextId,
		std::vector<ActorDefinition> actors)
		: m_revision(revision), m_rootActorId(rootId), m_nextLocalObjectId(nextId), m_actors(std::move(actors))
	{}

	DefinitionRevision m_revision;
	LocalObjectId m_rootActorId;
	LocalObjectId m_nextLocalObjectId;
	std::vector<ActorDefinition> m_actors;
};

