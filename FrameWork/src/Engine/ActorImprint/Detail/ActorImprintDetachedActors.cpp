#include "ActorImprintDetachedActors.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/ActorImprint/ActorImprint.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorMetadata.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include <unordered_set>

namespace ActorImprintDetail
{
std::optional<DetachedActors> CreateDetachedActors(
	const ActorImprint& definition,
	const SceneBase& destination,
	const ActorImprintReferenceCodec::ActorGuids* restoredGuids,
	const std::vector<ActorImprintPropertyOverrideTarget>* overrides,
	ActorImprintOverrideRevisionRelation revisionRelation)
{
	try
	{
		const auto& records = definition.GetActors();

		if (restoredGuids && restoredGuids->size() != records.size())
		{
			DBG("Restored GUID mapping must cover exactly the definition's Actors.");
			return std::nullopt;
		}

		std::unordered_map<LocalObjectId, const ActorImprintPropertyOverrideTarget*> remainingOverrides;

		if (overrides)
		{
			remainingOverrides.reserve(overrides->size());

			for (const auto& target : *overrides)
			{
				if (!ActorImprintPropertyOverrides::ValidateStructure(target))
				{
					return std::nullopt;
				}

				if (!remainingOverrides.emplace(target.targetLocalObjectId, &target).second)
				{
					DBG("Override targets require unique nonzero LocalObjectIDs.");
					return std::nullopt;
				}
			}
		}

		ActorImprintReferenceCodec::ActorGuids guids;
		std::unordered_set<Guid> usedGuids;
		guids.reserve(records.size());
		usedGuids.reserve(records.size());

		for (const auto& record : records)
		{
			Guid guid;

			if (restoredGuids)
			{
				const auto saved = restoredGuids->find(record.id);

				if (saved == restoredGuids->end())
				{
					DBG("Restored Actor GUID is missing.");
					return std::nullopt;
				}

				guid = saved->second;
			}
			else
			{
				guid = GuidGenerator::Generate();
			}

			if (!guid.IsValid())
			{
				DBG("Actor GUID must be nonzero.");
				return std::nullopt;
			}

			if (!usedGuids.insert(guid).second)
			{
				DBG("Actor GUID is duplicated in the instance input.");
				return std::nullopt;
			}

			if (!destination.FindActorHandle(guid).IsNull())
			{
				DBG("Actor GUID already exists in the destination Scene.");
				return std::nullopt;
			}

			guids.emplace(record.id, guid);
		}

		// Establish all identities before invoking any Component factory or setter.
		DetachedActors actors;
		actors.reserve(records.size());

		for (const auto& record : records)
		{
			auto actor = ActorFactory::RestoreActorShell({}, guids.at(record.id));

			if (!actor)
			{
				DBG("ActorFactory did not create an Actor shell.");
				return std::nullopt;
			}

			actors.push_back(std::move(actor));
		}

		ActorImprintReferenceCodec actorCodec(std::move(guids), ActorImprintReferenceMode::Instance);
		AssetReferenceCodec assetCodec;
		ReflectionRestoreContext restore{ .actorReferenceCodec = &actorCodec, .assetReferenceCodec = &assetCodec };
		ComponentRegistry& registry = ComponentRegistry::Get();
		auto RestoreProperties =
			[&](const TypeMetadata& metadata, std::type_index type, void* object, const nlohmann::json& properties)
		{
			if (ReflectionDeserializer::Deserialize(metadata, type, properties, object, restore))
			{
				return true;
			}

			return false;
		};
		auto CompleteProperties = [&](LocalObjectId id, const TypeMetadata& metadata, const nlohmann::json& defaults,
									  nlohmann::json& completed)
		{
			const auto target = remainingOverrides.find(id);

			if (target == remainingOverrides.end())
			{
				completed = defaults;
				return true;
			}

			if (!ActorImprintPropertyOverrides::Merge(metadata, defaults, *target->second,
				revisionRelation, completed, nullptr, nullptr))
			{
				return false;
			}

			remainingOverrides.erase(target);
			return true;
		};

		for (std::size_t i = 0; i < records.size(); ++i)
		{
			const auto& record = records[i];
			Actor& actor = *actors[i];

			nlohmann::json completed;

			if (!CompleteProperties(record.id, GetActorMetadata(), record.properties, completed) ||
				!RestoreProperties(GetActorMetadata(), typeid(Actor), &actor, completed))
			{
				return std::nullopt;
			}

			for (const auto& componentRecord : record.components)
			{
				const auto type = registry.GetTypeId(componentRecord.typeName);
				const auto* metadata = registry.GetMetadata(componentRecord.typeName);

				if (!type || *type != componentRecord.type || !metadata)
				{
					DBG("Component registration no longer matches the loaded definition.");
					return std::nullopt;
				}

				std::unique_ptr<Component> component(registry.Create(componentRecord.typeName));

				if (!component || typeid(*component) != *type)
				{
					DBG("Component factory did not create the registered type.");
					return std::nullopt;
				}

				Component* owned = actor.AddComponent(std::move(component));

				if (!owned)
				{
					DBG("Actor rejected the Component configuration.");
					return std::nullopt;
				}

				if (!CompleteProperties(componentRecord.id, *metadata, componentRecord.properties, completed) ||
					!RestoreProperties(*metadata, *type, owned, completed))
				{
					return std::nullopt;
				}
			}
		}

		if (!remainingOverrides.empty() && revisionRelation == ActorImprintOverrideRevisionRelation::Same)
		{
			DBG("Same-revision Override target does not exist in the definition.");
			return std::nullopt;
		}

		return std::move(actors);
	}
	catch (const std::exception& exception)
	{
		DBG("%s", exception.what());
		return std::nullopt;
	}
}
}
