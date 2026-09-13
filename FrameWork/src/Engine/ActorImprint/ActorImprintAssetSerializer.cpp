#include "ActorImprintAssetSerializer.h"
#include "ActorImprint.h"

nlohmann::json ActorImprintAssetSerializer::Serialize(const ActorImprint& imprint)
{
	using json = nlohmann::json;
	json actors = json::array();
	// The only constructor receives validated records sorted by LocalObjectID.
	for (const auto& actor : imprint.GetActors())
	{
		json components = json::array();
		for (const auto& component : actor.components)
		{
			components.push_back({ { "localObjectId", component.id }, { "type", component.typeName },
				{ "properties", component.properties } });
		}
		actors.push_back({ { "localObjectId", actor.id },
			{ "parentLocalObjectId", actor.parentId == 0 ? json(nullptr) : json(actor.parentId) },
			{ "properties", actor.properties }, { "components", std::move(components) } });
	}
	return { { "version", ActorImprint::SCHEMA_VERSION }, { "definitionRevision", imprint.GetRevision().ToString() },
		{ "rootActorLocalObjectId", imprint.GetRootActorId() }, { "nextLocalObjectId", imprint.GetNextLocalObjectId() },
		{ "actors", std::move(actors) } };
}

