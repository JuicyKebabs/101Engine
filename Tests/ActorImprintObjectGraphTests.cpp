#include "Engine/ActorImprint/Detail/ActorImprintObjectGraph.h"
#include "Engine/ActorImprint/LocalObjectId.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
	using json = nlohmann::json;
	using ActorImprintDetail::ObjectGraphError;
	using ActorImprintDetail::ValidateObjectGraph;
	int g_failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (condition)
		{
			std::cout << "[PASS] " << name << '\n';
			return;
		}
		std::cerr << "[FAIL] " << name << '\n';
		++g_failures;
	}

	// A graph-phase fixture, not a complete .imprint asset. Property/type/revision
	// validation will be tested through the asset deserializer after its contract
	// is settled. This test does not substitute for that integration pilot.
	json MakeGraph()
	{
		return {
			{ "rootActorLocalObjectId", 10 },
			{ "nextLocalObjectId", 100 },
			{ "actors", json::array({
				{ { "localObjectId", 10 }, { "parentLocalObjectId", nullptr },
					{ "components", json::array({ { { "localObjectId", 11 } }, { { "localObjectId", 12 } } }) } },
				{ { "localObjectId", 20 }, { "parentLocalObjectId", 10 },
					{ "components", json::array({ { { "localObjectId", 21 } } }) } },
				{ { "localObjectId", 30 }, { "parentLocalObjectId", 20 },
					{ "components", json::array({ { { "localObjectId", 31 } } }) } }
			}) }
		};
	}

	bool RejectedAt(const json& graph, const std::string& path)
	{
		const json before = graph;
		ObjectGraphError error;
		return !ValidateObjectGraph(graph, error) && error.path == path &&
			!error.message.empty() && graph == before;
	}

	void TestValidGraphs()
	{
		json graph = MakeGraph();
		const json before = graph;
		ObjectGraphError error{ "/previous", "previous error" };
		Check(ValidateObjectGraph(graph, error) && graph == before &&
			error.path.empty() && error.message.empty(),
			"Closed hierarchy validates without mutation and clears an old diagnostic");

		std::reverse(graph["actors"].begin(), graph["actors"].end());
		std::reverse(graph["actors"][2]["components"].begin(), graph["actors"][2]["components"].end());
		Check(ValidateObjectGraph(graph, error), "Child-first Actors and reordered Components are accepted");

		graph = MakeGraph();
		graph["actors"].erase(graph["actors"].begin() + 1, graph["actors"].end());
		Check(ValidateObjectGraph(graph, error), "One root with no child Actors is accepted");

		graph = MakeGraph();
		const LocalObjectId largest = (std::numeric_limits<LocalObjectId>::max)();
		graph["actors"][2]["components"][0]["localObjectId"] = largest - 1;
		graph["nextLocalObjectId"] = largest;
		Check(ValidateObjectGraph(graph, error), "IDs above signed 64-bit range retain unsigned precision");
		graph["actors"][2]["components"][0]["localObjectId"] = largest;
		Check(RejectedAt(graph, "/nextLocalObjectId"), "No ID increment may wrap past UINT64_MAX");
	}

	void TestIdentityFailures()
	{
		json graph = MakeGraph();
		graph["actors"][2]["localObjectId"] = 20;
		Check(RejectedAt(graph, "/actors/2/localObjectId"), "Duplicate Actor ID is rejected");
		graph = MakeGraph();
		graph["actors"][2]["components"][0]["localObjectId"] = 11;
		Check(RejectedAt(graph, "/actors/2/components/0/localObjectId"), "Components on different Actors share one ID namespace");
		graph = MakeGraph();
		graph["actors"][0]["components"][1]["localObjectId"] = 10;
		Check(RejectedAt(graph, "/actors/0/components/1/localObjectId"), "Component ID cannot collide with an Actor ID");
		graph = MakeGraph();
		graph["actors"][2]["localObjectId"] = 11;
		Check(RejectedAt(graph, "/actors/2/localObjectId"), "Actor ID cannot collide with an earlier Component ID");

		for (int nextId : { 31, 30 })
		{
			graph = MakeGraph();
			graph["nextLocalObjectId"] = nextId;
			Check(RejectedAt(graph, "/nextLocalObjectId"), "Next ID must exceed all issued IDs: " + std::to_string(nextId));
		}

		const std::vector<json> invalidIds{ nullptr, false, true, 0, -1, 1.5, "10", json::array(), json::object(),
			json::parse("18446744073709551616") };
		const std::vector<std::string> paths{
			"/rootActorLocalObjectId", "/nextLocalObjectId", "/actors/0/localObjectId",
			"/actors/0/components/0/localObjectId", "/actors/1/parentLocalObjectId"
		};
		for (const std::string& path : paths)
		{
			bool allRejected = true;
			for (const json& invalid : invalidIds)
			{
				// A null non-root parent is a hierarchy error at this same location.
				graph = MakeGraph();
				graph[json::json_pointer(path)] = invalid;
				allRejected &= RejectedAt(graph, path);
			}
			Check(allRejected, "Invalid ID values are rejected at " + path);
		}
	}

	void TestHierarchyFailures()
	{
		json graph = MakeGraph();
		graph["actors"] = json::array();
		Check(RejectedAt(graph, "/actors"), "Zero Actor definitions are rejected");
		graph = MakeGraph();
		graph["rootActorLocalObjectId"] = 11;
		Check(RejectedAt(graph, "/rootActorLocalObjectId"), "Root cannot identify a Component");
		graph["rootActorLocalObjectId"] = 99;
		Check(RejectedAt(graph, "/rootActorLocalObjectId"), "Missing root is rejected");
		graph = MakeGraph();
		graph["actors"][0]["parentLocalObjectId"] = 20;
		Check(RejectedAt(graph, "/actors/0/parentLocalObjectId"), "Declared root must have a null parent");
		graph = MakeGraph();
		graph["actors"][1]["parentLocalObjectId"] = nullptr;
		Check(RejectedAt(graph, "/actors/1/parentLocalObjectId"), "Second root is rejected");
		graph = MakeGraph();
		graph["actors"][1]["parentLocalObjectId"] = 99;
		Check(RejectedAt(graph, "/actors/1/parentLocalObjectId"), "External parent is rejected");
		graph["actors"][1]["parentLocalObjectId"] = 11;
		Check(RejectedAt(graph, "/actors/1/parentLocalObjectId"), "Parent cannot identify a Component");
		graph["actors"][1]["parentLocalObjectId"] = 20;
		Check(RejectedAt(graph, "/actors/1/parentLocalObjectId"), "Self-parent cycle is rejected");
		graph["actors"][1]["parentLocalObjectId"] = 30;
		Check(RejectedAt(graph, "/actors/1/parentLocalObjectId"), "Disconnected cycle is rejected even when a valid root exists");
	}

	void TestMalformedRecords()
	{
		Check(RejectedAt(nullptr, ""), "Non-object asset is rejected");
		const std::vector<std::string> requiredPaths{
			"/rootActorLocalObjectId", "/nextLocalObjectId", "/actors",
			"/actors/0/localObjectId", "/actors/0/parentLocalObjectId", "/actors/0/components",
			"/actors/0/components/0/localObjectId"
		};
		for (const std::string& path : requiredPaths)
		{
			json graph = MakeGraph();
			const json::json_pointer pointer(path);
			graph[pointer.parent_pointer()].erase(pointer.back());
			Check(RejectedAt(graph, path), "Missing required graph field is rejected at " + path);
		}
		for (const std::string& path : { "/actors", "/actors/0", "/actors/0/components", "/actors/0/components/0" })
		{
			json graph = MakeGraph();
			graph[json::json_pointer(path)] = 42;
			Check(RejectedAt(graph, path), "Malformed record is diagnosed at " + path);
		}
	}

	void TestDeepHierarchy()
	{
		constexpr LocalObjectId count = 8192;
		json graph{
			{ "rootActorLocalObjectId", 1 },
			{ "nextLocalObjectId", count + 1 },
			{ "actors", json::array() }
		};
		// Reverse order requires a full-depth traversal on the first visit.
		for (LocalObjectId id = count; id > 0; --id)
		{
			graph["actors"].push_back({
				{ "localObjectId", id },
				{ "parentLocalObjectId", id == 1 ? json(nullptr) : json(id - 1) },
				{ "components", json::array() }
			});
		}
		ObjectGraphError error;
		Check(ValidateObjectGraph(graph, error), "Deep child-first hierarchy validates without recursive stack growth");
	}
}

int main()
{
	TestValidGraphs();
	TestIdentityFailures();
	TestHierarchyFailures();
	TestMalformedRecords();
	TestDeepHierarchy();
	if (g_failures == 0)
	{
		std::cout << "All ActorImprintObjectGraph tests passed.\n";
		return 0;
	}
	std::cerr << g_failures << " ActorImprintObjectGraph test(s) failed.\n";
	return 1;
}
