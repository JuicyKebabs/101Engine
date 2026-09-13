#include "ActorImprintDefinitionExpander.h"

#include "ActorImprint.h"
#include "Detail/ActorImprintDetachedActors.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneActorBatch.h"

#include <unordered_map>

bool ActorImprintDefinitionExpander::Expand(const ActorImprint& definition,
	SceneBase& destination, ActorImprintDefinitionExpansion& outExpansion,
	ActorImprintDefinitionExpansionError* outError)
{
	if (outError) *outError = {};
	ActorImprintDefinitionExpansion candidate;
	auto fail = [&](LocalObjectId id, std::string path, std::string message)
	{
		if (outError) *outError = { id, std::move(path), std::move(message) };
		return false;
	};

	try
	{
		if (!destination.GetAllActors().empty() || destination.IsStructuralMutationBlocked())
			return fail(InvalidLocalObjectId, {}, "Working Scene must be empty and idle before definition expansion.");

		ActorImprintDetail::DetachedActorsError detachedError;
		auto detached = ActorImprintDetail::CreateDetachedActors(definition, destination, nullptr, detachedError);
		if (!detached)
			return fail(detachedError.objectId, std::move(detachedError.path), std::move(detachedError.message));

		SceneActorBatch batch(destination);
		batch.Stage(std::move(*detached));
		const auto& records = definition.GetActors();
		if (records.size() != batch.Handles().size())
			return fail(InvalidLocalObjectId, {}, "Definition expansion Actor count is inconsistent.");

		std::unordered_map<LocalObjectId, Actor*> actors;
		actors.reserve(records.size());
		candidate.actorGuids.reserve(records.size());
		for (std::size_t i = 0; i < records.size(); ++i)
		{
			Actor* actor = batch.Candidate().ResolveActor(batch.Handles()[i]);
			if (!actor || !actors.emplace(records[i].id, actor).second ||
				!candidate.actorGuids.emplace(records[i].id, actor->GetGuid()).second)
				return fail(records[i].id, {}, "Definition expansion Actor identity is invalid.");
			if (records[i].id == definition.GetRootActorId()) candidate.root = actor;

			std::unordered_map<std::string, std::size_t> occurrences;
			for (const auto& componentDefinition : records[i].components)
			{
				Component* component = actor->GetComponentByExactType(
					componentDefinition.type, occurrences[componentDefinition.typeName]++);
				if (!component || !candidate.components.emplace(componentDefinition.id, component).second)
					return fail(componentDefinition.id, {}, "Definition expansion Component identity is invalid.");
			}
		}
		if (!candidate.root) return fail(definition.GetRootActorId(), {}, "Definition root was not expanded.");

		for (const auto& record : records)
		{
			if (record.parentId == InvalidLocalObjectId) continue;
			const auto child = actors.find(record.id);
			const auto parent = actors.find(record.parentId);
			if (child == actors.end() || parent == actors.end() || !batch.SetParent(child->second, parent->second))
				return fail(record.id, {}, "Definition hierarchy could not be expanded.");
		}

		std::string validationError;
		if (!batch.ResolveAndValidate(candidate.root, nullptr, validationError))
			return fail(InvalidLocalObjectId, {}, std::move(validationError));
		batch.PrepareCommit(candidate.root, nullptr);
		batch.Commit();
		batch.Attach();
		outExpansion = std::move(candidate);
		return true;
	}
	catch (const std::exception& error)
	{
		return fail(InvalidLocalObjectId, {}, error.what());
	}
}
