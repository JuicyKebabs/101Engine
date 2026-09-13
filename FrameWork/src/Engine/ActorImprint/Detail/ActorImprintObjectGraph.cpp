#include "ActorImprintObjectGraph.h"
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

	bool Fail(ObjectGraphError& error, std::string path, std::string message)
	{
		error = { std::move(path), std::move(message) };
		return false;
	}

	bool ReadId(const json& value, LocalObjectId& outId)
	{
		if (value.is_number_unsigned())
		{
			outId = value.get<LocalObjectId>();
			return outId != 0;
		}

		if (!value.is_number_integer()) return false;
		const auto signedId = value.get<std::int64_t>();
		if (signedId <= 0) return false;
		outId = static_cast<LocalObjectId>(signedId);
		return true;
	}

	bool ReadRequiredId(
		const json& object,
		const char* member,
		const std::string& objectPath,
		LocalObjectId& outId,
		ObjectGraphError& error)
	{
		const auto entry = object.find(member);
		if (entry == object.end() || !ReadId(*entry, outId))
		{
			return Fail(error, objectPath + '/' + member,
				"Expected a nonzero unsigned 64-bit object ID.");
		}
		return true;
	}

	struct ActorNode
	{
		LocalObjectId id = 0;
		LocalObjectId parentId = 0;
	};
}

bool ValidateObjectGraph(const nlohmann::json& asset, ObjectGraphError& outError)
{
	outError = {};
	if (!asset.is_object()) return Fail(outError, "", "Asset must be an object.");

	LocalObjectId rootId = 0;
	LocalObjectId nextId = 0;
	if (!ReadRequiredId(asset, "rootActorLocalObjectId", "", rootId, outError) ||
		!ReadRequiredId(asset, "nextLocalObjectId", "", nextId, outError)) return false;

	const auto actors = asset.find("actors");
	if (actors == asset.end() || !actors->is_array() || actors->empty())
	{
		return Fail(outError, "/actors", "Expected a nonempty Actor array.");
	}

	std::vector<ActorNode> nodes;
	nodes.reserve(actors->size());
	std::unordered_map<LocalObjectId, std::size_t> actorIndices;
	std::unordered_set<LocalObjectId> objectIds;
	LocalObjectId greatestId = 0;

	auto RegisterId = [&](LocalObjectId id, const std::string& path)
	{
		if (!objectIds.insert(id).second)
		{
			return Fail(outError, path, "Actor and Component object IDs must be unique within the asset.");
		}
		if (id > greatestId) greatestId = id;
		return true;
	};

	// Keep original array indices for diagnostics; never require parents first.
	for (std::size_t i = 0; i < actors->size(); ++i)
	{
		const json& actor = (*actors)[i];
		const std::string path = "/actors/" + std::to_string(i);
		if (!actor.is_object()) return Fail(outError, path, "Actor record must be an object.");

		ActorNode node;
		if (!ReadRequiredId(actor, "localObjectId", path, node.id, outError) ||
			!RegisterId(node.id, path + "/localObjectId")) return false;

		const auto parent = actor.find("parentLocalObjectId");
		if (parent == actor.end() || (!parent->is_null() && !ReadId(*parent, node.parentId)))
		{
			return Fail(outError, path + "/parentLocalObjectId", "Expected null or a nonzero Actor object ID.");
		}

		const auto components = actor.find("components");
		if (components == actor.end() || !components->is_array())
		{
			return Fail(outError, path + "/components", "Expected a Component array.");
		}
		for (std::size_t j = 0; j < components->size(); ++j)
		{
			const json& component = (*components)[j];
			const std::string componentPath = path + "/components/" + std::to_string(j);
			if (!component.is_object()) return Fail(outError, componentPath, "Component record must be an object.");
			LocalObjectId componentId = 0;
			if (!ReadRequiredId(component, "localObjectId", componentPath, componentId, outError) ||
				!RegisterId(componentId, componentPath + "/localObjectId")) return false;
		}

		actorIndices.emplace(node.id, i);
		nodes.push_back(node);
	}

	if (nextId <= greatestId)
	{
		return Fail(outError, "/nextLocalObjectId", "Next object ID must exceed every Actor and Component ID.");
	}
	if (!actorIndices.contains(rootId))
	{
		return Fail(outError, "/rootActorLocalObjectId", "Root must identify an Actor in this asset.");
	}

	const std::size_t noParent = nodes.size();
	std::vector<std::size_t> parents(nodes.size(), noParent);
	for (std::size_t i = 0; i < nodes.size(); ++i)
	{
		const auto& node = nodes[i];
		const std::string path = "/actors/" + std::to_string(i) + "/parentLocalObjectId";
		if ((node.id == rootId) != (node.parentId == 0))
		{
			return Fail(outError, path, "Only the declared root Actor must have a null parent.");
		}
		if (node.parentId == 0) continue;
		const auto parent = actorIndices.find(node.parentId);
		if (parent == actorIndices.end())
		{
			return Fail(outError, path, "Parent must identify an Actor in this asset.");
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
			return Fail(outError, "/actors/" + std::to_string(current) + "/parentLocalObjectId",
				"Actor hierarchy contains a cycle.");
		}
		for (std::size_t visited : trail) states[visited] = VisitState::Visited;
	}

	return true;
}

}
