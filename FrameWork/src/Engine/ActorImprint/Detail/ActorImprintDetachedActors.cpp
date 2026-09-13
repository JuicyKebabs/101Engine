#include "ActorImprintDetachedActors.h"
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
std::optional<DetachedActors> CreateDetachedActors(const ActorImprint& definition,
	const SceneBase& destination, const ActorImprintReferenceCodec::ActorGuids* restoredGuids,
	const std::vector<ActorImprintPropertyOverrideTarget>* overrides,
	ActorImprintOverrideRevisionRelation revisionRelation,
	DetachedActorsError& outError)
{
	outError = {};
	auto Fail = [&](LocalObjectId id, std::string path, std::string message) -> std::optional<DetachedActors>
	{
		outError = { id, std::move(path), std::move(message) };
		return std::nullopt;
	};
	LocalObjectId currentObject = 0;
	try
	{
		const auto& records = definition.GetActors();
		if (restoredGuids && restoredGuids->size() != records.size())
			return Fail(0, "", "Restored GUID mapping must cover exactly the definition's Actors.");
		std::unordered_map<LocalObjectId, const ActorImprintPropertyOverrideTarget*> remainingOverrides;
		if (overrides)
		{
			remainingOverrides.reserve(overrides->size());
			for (const auto& target : *overrides)
			{
				ActorImprintPropertyOverrideError overrideError;
				if (!ActorImprintPropertyOverrides::ValidateStructure(target, &overrideError))
					return Fail(target.targetLocalObjectId, overrideError.path, overrideError.message);
				if (!remainingOverrides.emplace(target.targetLocalObjectId, &target).second)
					return Fail(target.targetLocalObjectId, "", "Override targets require unique nonzero LocalObjectIDs.");
			}
		}

		ActorImprintReferenceCodec::ActorGuids guids;
		std::unordered_set<Guid> usedGuids;
		guids.reserve(records.size());
		usedGuids.reserve(records.size());
		for (const auto& record : records)
		{
			currentObject = record.id;
			Guid guid;
			if (restoredGuids)
			{
				const auto saved = restoredGuids->find(record.id);
				if (saved == restoredGuids->end()) return Fail(record.id, "", "Restored Actor GUID is missing.");
				guid = saved->second;
			}
			else guid = GuidGenerator::Generate();
			if (!guid.IsValid()) return Fail(record.id, "", "Actor GUID must be nonzero.");
			if (!usedGuids.insert(guid).second) return Fail(record.id, "", "Actor GUID is duplicated in the instance input.");
			if (!destination.FindActorHandle(guid).IsNull())
				return Fail(record.id, "", "Actor GUID already exists in the destination Scene.");
			guids.emplace(record.id, guid);
		}

		// Establish all identities before invoking any Component factory or setter.
		DetachedActors actors;
		actors.reserve(records.size());
		for (const auto& record : records)
		{
			currentObject = record.id;
			auto actor = ActorFactory::RestoreActorShell({}, guids.at(record.id));
			if (!actor) return Fail(record.id, "", "ActorFactory did not create an Actor shell.");
			actors.push_back(std::move(actor));
		}
		ActorImprintReferenceCodec actorCodec(std::move(guids), ActorImprintReferenceMode::Instance);
		AssetReferenceCodec assetCodec;
		ReflectionRestoreContext restore{ .actorReferenceCodec = &actorCodec, .assetReferenceCodec = &assetCodec };
		ComponentRegistry& registry = ComponentRegistry::Get();
		ReflectionError reflectionError;
		auto RestoreProperties = [&](const TypeMetadata& metadata, std::type_index type, void* object, const nlohmann::json& properties)
		{
			if (ReflectionDeserializer::Deserialize(metadata, type, properties, object, restore, &reflectionError)) return true;
			outError = { currentObject, "/properties" + (reflectionError.path ? reflectionError.path->ToString() : ""), reflectionError.message };
			return false;
		};
		auto CompleteProperties = [&](LocalObjectId id, const TypeMetadata& metadata,
			const nlohmann::json& defaults, nlohmann::json& completed)
		{
			const auto target = remainingOverrides.find(id);
			if (target == remainingOverrides.end())
			{
				completed = defaults;
				return true;
			}
			ActorImprintPropertyOverrideError overrideError;
			if (!ActorImprintPropertyOverrides::Merge(metadata, defaults, *target->second,
				revisionRelation, completed, nullptr, nullptr, &overrideError))
			{
				outError = { id, overrideError.path, overrideError.message };
				return false;
			}
			remainingOverrides.erase(target);
			return true;
		};

		for (std::size_t i = 0; i < records.size(); ++i)
		{
			const auto& record = records[i];
			Actor& actor = *actors[i];
			currentObject = record.id;
			nlohmann::json completed;
			if (!CompleteProperties(record.id, GetActorMetadata(), record.properties, completed) ||
				!RestoreProperties(GetActorMetadata(), typeid(Actor), &actor, completed)) return std::nullopt;
			for (const auto& componentRecord : record.components)
			{
				currentObject = componentRecord.id;
				const auto type = registry.GetTypeId(componentRecord.typeName);
				const auto* metadata = registry.GetMetadata(componentRecord.typeName);
				if (!type || *type != componentRecord.type || !metadata)
					return Fail(currentObject, "/type", "Component registration no longer matches the loaded definition.");
				std::unique_ptr<Component> component(registry.Create(componentRecord.typeName));
				if (!component || typeid(*component) != *type)
					return Fail(currentObject, "/type", "Component factory did not create the registered type.");
				Component* owned = actor.AddComponent(std::move(component));
				if (!owned) return Fail(currentObject, "/type", "Actor rejected the Component configuration.");
				if (!CompleteProperties(componentRecord.id, *metadata, componentRecord.properties, completed) ||
					!RestoreProperties(*metadata, *type, owned, completed)) return std::nullopt;
			}
		}
		if (!remainingOverrides.empty() && revisionRelation == ActorImprintOverrideRevisionRelation::Same)
			return Fail(remainingOverrides.begin()->first, "", "Same-revision Override target does not exist in the definition.");
		return std::move(actors);
	}
	catch (const std::exception& exception)
	{
		return Fail(currentObject, "", exception.what());
	}
}
}
