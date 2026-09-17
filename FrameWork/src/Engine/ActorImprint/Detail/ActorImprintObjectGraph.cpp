#include "ActorImprintObjectGraph.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/ActorImprint/LocalObjectId.h"
#include "nlohmann/json.hpp"
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ActorImprintDetail
{
namespace
{
	using json = nlohmann::json;

	bool ReadId(const json& value, LocalObjectId& outId)
	{
		if (value.is_number_unsigned())
		{
			outId = value.get<LocalObjectId>();
			return outId != 0;
		}

		if (!value.is_number_integer())
		{
			return false;
		}

		const auto signedId = value.get<std::int64_t>();

		if (signedId <= 0)
		{
			return false;
		}

		outId = static_cast<LocalObjectId>(signedId);
		return true;
	}

	bool ReadRequiredId(const json& object, const char* member, LocalObjectId& outId)
	{
		const auto entry = object.find(member);

		if (entry == object.end() || !ReadId(*entry, outId))
		{
			DBG("Expected a nonzero unsigned 64-bit object ID.");
			return false;
		}

		return true;
	}

	struct ActorNode
	{
		LocalObjectId id = 0;
		LocalObjectId parentId = 0;
	};
}

bool ValidateObjectGraph(const nlohmann::json& asset)
{
	if (!asset.is_object())
	{
		DBG("Asset must be an object.");
		return false;
	}

	LocalObjectId rootId = 0;
	LocalObjectId nextId = 0;

	if (!ReadRequiredId(asset, "rootActorLocalObjectId", rootId) ||
		!ReadRequiredId(asset, "nextLocalObjectId", nextId))
	{
		return false;
	}

	const auto actors = asset.find("actors");

	if (actors == asset.end() || !actors->is_array() || actors->empty())
	{
		DBG("Expected a nonempty Actor array.");
		return false;
	}

	std::vector<ActorNode> nodes;
	nodes.reserve(actors->size());
	std::unordered_map<LocalObjectId, std::size_t> actorIndices;
	std::unordered_set<LocalObjectId> objectIds;
	LocalObjectId greatestId = 0;

	auto RegisterId = [&](LocalObjectId id)
	{
		if (!objectIds.insert(id).second)
		{
			DBG("Actor and Component object IDs must be unique within the asset.");
			return false;
		}

		if (id > greatestId)
		{
			greatestId = id;
		}

		return true;
	};

	// Actor records need not list parents first.
	for (std::size_t i = 0; i < actors->size(); ++i)
	{
		const json& actor = (*actors)[i];

		if (!actor.is_object())
		{
			DBG("Actor record must be an object.");
			return false;
		}

		ActorNode node;

		if (!ReadRequiredId(actor, "localObjectId", node.id) ||
			!RegisterId(node.id))
		{
			return false;
		}

		const auto parent = actor.find("parentLocalObjectId");

		if (parent == actor.end() || (!parent->is_null() && !ReadId(*parent, node.parentId)))
		{
			DBG("Expected null or a nonzero Actor object ID.");
			return false;
		}

		const auto components = actor.find("components");

		if (components == actor.end() || !components->is_array())
		{
			DBG("Expected a Component array.");
			return false;
		}

		for (std::size_t j = 0; j < components->size(); ++j)
		{
			const json& component = (*components)[j];

			if (!component.is_object())
			{
				DBG("Component record must be an object.");
				return false;
			}

			LocalObjectId componentId = 0;

			if (!ReadRequiredId(component, "localObjectId", componentId) ||
				!RegisterId(componentId))
			{
				return false;
			}
		}

		actorIndices.emplace(node.id, i);
		nodes.push_back(node);
	}

	if (nextId <= greatestId)
	{
		DBG("Next object ID must exceed every Actor and Component ID.");
		return false;
	}

	if (!actorIndices.contains(rootId))
	{
		DBG("Root must identify an Actor in this asset.");
		return false;
	}

	const std::size_t noParent = nodes.size();
	std::vector<std::size_t> parents(nodes.size(), noParent);

	for (std::size_t i = 0; i < nodes.size(); ++i)
	{
		const auto& node = nodes[i];

		if ((node.id == rootId) != (node.parentId == 0))
		{
			DBG("Only the declared root Actor must have a null parent.");
			return false;
		}

		if (node.parentId == 0)
		{
			continue;
		}

		const auto parent = actorIndices.find(node.parentId);

		if (parent == actorIndices.end())
		{
			DBG("Parent must identify an Actor in this asset.");
			return false;
		}

		parents[i] = parent->second;
	}

	// With one root and no missing parents, acyclicity also proves connectivity.
	// Walk iteratively so a deeply nested file cannot exhaust the call stack.
	enum class VisitState { Unvisited, Visiting, Visited };
	std::vector<VisitState> states(nodes.size(), VisitState::Unvisited);
	std::vector<std::size_t> trail;

	for (std::size_t i = 0; i < nodes.size(); ++i)
	{
		std::size_t current = i;
		trail.clear();
		while (current != noParent && states[current] == VisitState::Unvisited)
		{
			states[current] = VisitState::Visiting;
			trail.push_back(current);
			current = parents[current];
		}

		if (current != noParent && states[current] == VisitState::Visiting)
		{
			DBG("Actor hierarchy contains a cycle.");
			return false;
		}

		for (std::size_t visited : trail)
		{
			states[visited] = VisitState::Visited;
		}
	}

	return true;
}
}
