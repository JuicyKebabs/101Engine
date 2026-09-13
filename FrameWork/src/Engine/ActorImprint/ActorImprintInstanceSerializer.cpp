#include "ActorImprintInstanceSerializer.h"
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

namespace
{
	using ErrorCode = ActorImprintInstanceSerializationErrorCode;

	bool Fail(ActorImprintInstanceSerializationError* error, ErrorCode code,
		LocalObjectId id, std::string path, std::string message)
	{
		if (error) *error = { code, id, std::move(path), std::move(message) };
		return false;
	}
}

bool ActorImprintInstanceSerializer::Capture(const SceneBase& scene, ActorHandle root,
	ActorImprintSerializedInstanceRecord& outRecord, ActorImprintInstanceSerializationError* outError)
{
	if (outError) *outError = {};
	try
	{
		const auto* context = scene.GetEngineContext();
		const auto* record = scene.GetImprintInstances().FindInstance(root);
		if (!context || !context->pActorImprintSystem || !context->pAssetManager)
			return Fail(outError, ErrorCode::InvalidScene, 0, {}, "Scene has no ActorImprint or Asset system context.");
		if (!record || record->destroying || !scene.ValidateInstanceDestruction(root))
			return Fail(outError, ErrorCode::InvalidInstance, 0, {}, "Instance is incomplete, destroying, or structurally invalid.");
		ActorImprintSystem& system = *context->pActorImprintSystem;
		const ActorImprint* definition = system.Resolve(record->imprint);
		if (!definition || record->assetGuid != system.GetAssetGuid(record->imprint) ||
			record->sourceRevision != definition->GetRevision())
			return Fail(outError, ErrorCode::InvalidDefinition, 0, {}, "Instance provenance does not match the current definition.");

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
				return Fail(outError, ErrorCode::InvalidIdentity, actorDefinition.id, {}, "Instance Actor identity is missing or invalid.");
			candidate.actorGuids.push_back({ actorDefinition.id, identity->second.guid });
		}
		Actor* rootActor = scene.ResolveActor(root);
		if (!rootActor || rootActor->GetGuid() != candidate.rootActorGuid)
			return Fail(outError, ErrorCode::InvalidIdentity, record->rootId, {}, "Instance root identity is inconsistent.");
		if (Actor* parent = rootActor->GetParent()) candidate.externalParentActorGuid = parent->GetGuid();

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
				if (!property.GetSerializationMetadata()) continue;
				PropertyValue value;
				if (!property.Read(type, object, value) || !property.ValidateValue(value))
					return Fail(outError, ErrorCode::InvalidProperty, id, property.GetPath().ToString(),
						"Live property is outside its supported persistence domain.");
			}
			ReflectionError validationError;
			if (!metadata.Validate(type, object, &validationError))
				return Fail(outError, ErrorCode::InvalidProperty, id,
					validationError.path ? validationError.path->ToString() : std::string{}, validationError.message);
			nlohmann::json current;
			ReflectionError reflectionError;
			if (!ReflectionSerializer::Serialize(metadata, type, object, current, save, &reflectionError))
			{
				return Fail(outError, ErrorCode::InvalidProperty, id,
					reflectionError.path ? reflectionError.path->ToString() : std::string{}, reflectionError.message);
			}
			ActorImprintPropertyOverrideTarget target;
			ActorImprintPropertyOverrideError overrideError;
			if (!ActorImprintPropertyOverrides::Diff(metadata, id, defaults, current, target, &overrideError))
				return Fail(outError, ErrorCode::InvalidProperty, id, overrideError.path, overrideError.message);
			if (!target.properties.empty()) candidate.propertyOverrides.push_back(std::move(target));
			return true;
		};

		ComponentRegistry& componentRegistry = ComponentRegistry::Get();
		for (const auto& actorDefinition : definition->GetActors())
		{
			Actor* actor = scene.GetImprintInstances().ResolveActor(root, actorDefinition.id);
			if (!actor || !CaptureProperties(actorDefinition.id, GetActorMetadata(), typeid(Actor),
				actor, actorDefinition.properties)) return false;
			for (const auto& componentDefinition : actorDefinition.components)
			{
				Component* component = scene.GetImprintInstances().ResolveComponent(root, componentDefinition.id);
				const TypeMetadata* metadata = componentRegistry.GetMetadata(componentDefinition.typeName);
				if (!component || !metadata || metadata->GetType() != componentDefinition.type ||
					std::type_index(typeid(*component)) != componentDefinition.type)
					return Fail(outError, ErrorCode::InvalidDefinition, componentDefinition.id, {},
						"Component instance and registered persistence metadata do not match the definition.");
				if (!CaptureProperties(componentDefinition.id, *metadata, componentDefinition.type,
					component, componentDefinition.properties)) return false;
			}
		}
		nlohmann::json validationJson;
		ActorImprintInstanceRecordError recordError;
		if (!ActorImprintInstanceRecordWriter::Write(candidate, validationJson, &recordError))
			return Fail(outError, ErrorCode::InvalidRecord, recordError.targetLocalObjectId,
				recordError.path, recordError.message);
		outRecord = std::move(candidate);
		return true;
	}
	catch (const std::exception& error)
	{
		return Fail(outError, ErrorCode::InvalidInstance, 0, {}, error.what());
	}
}

bool ActorImprintInstanceSerializer::Serialize(const SceneBase& scene, ActorHandle root,
	nlohmann::json& outJson, ActorImprintInstanceSerializationError* outError)
{
	ActorImprintSerializedInstanceRecord record;
	if (!Capture(scene, root, record, outError)) return false;
	nlohmann::json candidate;
	ActorImprintInstanceRecordError recordError;
	if (!ActorImprintInstanceRecordWriter::Write(record, candidate, &recordError))
		return Fail(outError, ErrorCode::InvalidRecord, recordError.targetLocalObjectId,
			recordError.path, recordError.message);
	outJson = std::move(candidate);
	return true;
}
