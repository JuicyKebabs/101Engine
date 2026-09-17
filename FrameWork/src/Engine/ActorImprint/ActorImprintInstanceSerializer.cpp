#include "ActorImprintInstanceSerializer.h"
#include "Engine/Core/Debug/Debug.h"
#include "ActorImprintInstanceRecordCodec.h"
#include "ActorImprintInstanceRegistry.h"
#include "ActorImprintPropertyOverrides.h"
#include "ActorImprintReferenceCodec.h"
#include "ActorImprintSystem.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorMetadata.h"
#include "Engine/Component/Component.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneActorReferenceContext.h"
#include "Engine/Scene/SceneBase.h"

bool ActorImprintInstanceSerializer::Capture(
	const SceneBase& scene,
	ActorHandle root,
	ActorImprintSerializedInstanceRecord& outRecord)
{
	try
	{
		const auto* context = scene.GetEngineContext();
		const auto* record = scene.GetImprintInstances().FindInstance(root);

		if (!context || !context->pActorImprintSystem || !context->pAssetManager)
		{
			DBG("Scene has no ActorImprint or Asset system context.");
			return false;
		}

		if (!record || record->destroying || !scene.ValidateInstanceDestruction(root))
		{
			DBG("Instance is incomplete, destroying, or structurally invalid.");
			return false;
		}

		ActorImprintSystem& system = *context->pActorImprintSystem;
		const ActorImprint* definition = system.Resolve(record->imprint);

		if (!definition || record->assetGuid != system.GetAssetGuid(record->imprint) ||
			record->sourceRevision != definition->GetRevision())
		{
			DBG("Instance provenance does not match the current definition.");
			return false;
		}

		ActorImprintReferenceCodec::ActorGuids localGuids;
		localGuids.reserve(record->actors.size());
		ActorImprintSerializedInstanceRecord candidate;
		candidate.assetGuid = record->assetGuid;
		candidate.sourceDefinitionRevision = definition->GetRevision();
		candidate.rootActorGuid = record->actors.at(record->rootId).guid;
		candidate.actorGuids.reserve(definition->GetActors().size());

		for (const auto& actorDefinition : definition->GetActors())
		{
			const auto identity = record->actors.find(actorDefinition.id);

			if (identity == record->actors.end() || !identity->second.guid.IsValid() ||
				!localGuids.emplace(actorDefinition.id, identity->second.guid).second)
			{
				DBG("Instance Actor identity is missing or invalid.");
				return false;
			}

			candidate.actorGuids.push_back({actorDefinition.id, identity->second.guid});
		}

		Actor* rootActor = scene.ResolveActor(root);

		if (!rootActor || rootActor->GetGuid() != candidate.rootActorGuid)
		{
			DBG("Instance root identity is inconsistent.");
			return false;
		}

		if (Actor* parent = rootActor->GetParent())
		{
			candidate.externalParentActorGuid = parent->GetGuid();
		}

		ActorImprintReferenceCodec actorCodec(std::move(localGuids), ActorImprintReferenceMode::Instance);
		SceneActorReferenceContext actorContext(scene);
		AssetReferenceCodec assetCodec;
		AssetManagerAssetReferenceContext assetContext(*context->pAssetManager);
		ReflectionSaveContext save{
			.actorReferenceCodec = &actorCodec,
			.actorReferenceContext = &actorContext,
			.assetReferenceCodec = &assetCodec,
			.assetReferenceContext = &assetContext,
		};

		auto CaptureProperties = [&](LocalObjectId id, const TypeMetadata& metadata,
			std::type_index type, const void* object, const nlohmann::json& defaults)
		{
			for (const PropertyMetadata& property : metadata.GetProperties())
			{
				if (!property.GetSerializationMetadata())
				{
					continue;
				}

				PropertyValue value;

				if (!property.Read(type, object, value) || !property.ValidateValue(value))
				{
					DBG("Live property is outside its supported persistence domain.");
					return false;
				}
			}

			if (!metadata.Validate(type, object))
			{
				return false;
			}

			nlohmann::json current;

			if (!ReflectionSerializer::Serialize(metadata, type, object, current, save))
			{
				return false;
			}

			ActorImprintPropertyOverrideTarget target;

			if (!ActorImprintPropertyOverrides::Diff(metadata, id, defaults, current, target))
			{
				return false;
			}

			if (!target.properties.empty())
			{
				candidate.propertyOverrides.push_back(std::move(target));
			}

			return true;
		};

		ComponentRegistry& componentRegistry = ComponentRegistry::Get();

		for (const auto& actorDefinition : definition->GetActors())
		{
			Actor* actor = scene.GetImprintInstances().ResolveActor(root, actorDefinition.id);

			if (!actor || !CaptureProperties(actorDefinition.id, GetActorMetadata(), typeid(Actor),
							  actor, actorDefinition.properties))
			{
				return false;
			}

			for (const auto& componentDefinition : actorDefinition.components)
			{
				Component* component = scene.GetImprintInstances().ResolveComponent(root, componentDefinition.id);
				const TypeMetadata* metadata = componentRegistry.GetMetadata(componentDefinition.typeName);

				if (!component || !metadata || metadata->GetType() != componentDefinition.type ||
					std::type_index(typeid(*component)) != componentDefinition.type)
				{
					DBG("Component instance and registered persistence metadata do not match the definition.");
					return false;
				}

				if (!CaptureProperties(componentDefinition.id, *metadata, componentDefinition.type,
						component, componentDefinition.properties))
				{
					return false;
				}
			}
		}

		nlohmann::json validationJson;

		if (!ActorImprintInstanceRecordWriter::Write(candidate, validationJson))
		{
			return false;
		}

		outRecord = std::move(candidate);
		return true;
	}
	catch (const std::exception& error)
	{
		DBG("%s", error.what());
		return false;
	}
}

bool ActorImprintInstanceSerializer::Serialize(
	const SceneBase& scene,
	ActorHandle root,
	nlohmann::json& outJson)
{
	ActorImprintSerializedInstanceRecord record;

	if (!Capture(scene, root, record))
	{
		return false;
	}

	nlohmann::json candidate;

	if (!ActorImprintInstanceRecordWriter::Write(record, candidate))
	{
		return false;
	}

	outJson = std::move(candidate);
	return true;
}
