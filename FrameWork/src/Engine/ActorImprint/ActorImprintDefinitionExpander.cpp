#include "ActorImprintDefinitionExpander.h"
#include "Engine/Core/Debug/Debug.h"

#include "ActorImprint.h"
#include "Detail/ActorImprintDetachedActors.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneActorBatch.h"

#include <unordered_map>

bool ActorImprintDefinitionExpander::Expand(
	const ActorImprint& definition,
	SceneBase& destination,
	ActorImprintDefinitionExpansion& outExpansion)
{
	ActorImprintDefinitionExpansion candidate;

	try
	{
		if (!destination.GetAllActors().empty() || destination.IsStructuralMutationBlocked())
		{
			DBG("Working Scene must be empty and idle before definition expansion.");
			return false;
		}

		auto detached = ActorImprintDetail::CreateDetachedActors(definition, destination, nullptr);

		if (!detached)
		{
			return false;
		}

		SceneActorBatch batch(destination);
		batch.Stage(std::move(*detached));
		const auto& records = definition.GetActors();

		if (records.size() != batch.Handles().size())
		{
			DBG("Definition expansion Actor count is inconsistent.");
			return false;
		}

		std::unordered_map<LocalObjectId, Actor*> actors;
		actors.reserve(records.size());
		candidate.actorGuids.reserve(records.size());

		for (std::size_t i = 0; i < records.size(); ++i)
		{
			Actor* actor = batch.Candidate().ResolveActor(batch.Handles()[i]);

			if (!actor || !actors.emplace(records[i].id, actor).second ||
				!candidate.actorGuids.emplace(records[i].id, actor->GetGuid()).second)
			{
				DBG("Definition expansion Actor identity is invalid.");
				return false;
			}

			if (records[i].id == definition.GetRootActorId())
			{
				candidate.root = actor;
			}

			std::unordered_map<std::string, std::size_t> occurrences;

			for (const auto& componentDefinition : records[i].components)
			{
				Component* component = actor->GetComponentByExactType(
					componentDefinition.type, occurrences[componentDefinition.typeName]++);

				if (!component || !candidate.components.emplace(componentDefinition.id, component).second)
				{
					DBG("Definition expansion Component identity is invalid.");
					return false;
				}
			}
		}

		if (!candidate.root)
		{
			DBG("Definition root was not expanded.");
			return false;
		}

		for (const auto& record : records)
		{
			if (record.parentId == InvalidLocalObjectId)
			{
				continue;
			}

			const auto child = actors.find(record.id);
			const auto parent = actors.find(record.parentId);

			if (child == actors.end() || parent == actors.end() || !batch.SetParent(child->second, parent->second))
			{
				DBG("Definition hierarchy could not be expanded.");
				return false;
			}
		}

		if (!batch.ResolveAndValidate(candidate.root, nullptr))
		{
			return false;
		}

		batch.PrepareCommit(candidate.root, nullptr);
		batch.Commit();
		batch.Attach();
		outExpansion = std::move(candidate);
		return true;
	}
	catch (const std::exception& error)
	{
		DBG("%s", error.what());
		return false;
	}
}
