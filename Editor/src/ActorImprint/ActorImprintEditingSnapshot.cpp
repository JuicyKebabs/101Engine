#include "ActorImprintEditingSnapshot.h"

#include "ActorImprintEditingObjectMap.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorMetadata.h"
#include "Engine/ActorImprint/ActorImprint.h"
#include "Engine/ActorImprint/ActorImprintAssetSerializer.h"
#include "Engine/ActorImprint/ActorImprintReferenceCodec.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneActorReferenceContext.h"
#include "Engine/Scene/SceneBase.h"

#include <unordered_map>

std::unique_ptr<const ActorImprint> ActorImprintEditingSnapshot::CreateDefault(
	const AssetManager& assets, EngineContext& engineContext, nlohmann::json& outJson,
	ActorImprintEditingSnapshotError* outError)
{
	if (outError) *outError = {};
	auto fail = [&](std::string message) -> std::unique_ptr<const ActorImprint>
	{
		if (outError) *outError = { ActorImprintAssetErrorCode::InvalidObjectGraph,
			InvalidLocalObjectId, {}, std::move(message) };
		return nullptr;
	};
	if (engineContext.pAssetManager != &assets)
		return fail("EngineContext does not own the supplied AssetManager.");

	auto scene = std::make_unique<SceneBase>();
	scene->Initialize(engineContext);
	Actor* root = scene->AddRootActor(ActorFactory::CreateEmptyActor(
		Actor::InitDesc(true, TAG_NONE, "Root")));
	if (!root || !scene->EnableSingleRootClosedSubtreePolicy())
	{
		scene->Finalize();
		return fail("Could not create the default single-root Working Scene.");
	}
	Transform* transform = root->GetComponentByClass<Transform>();
	if (!transform)
	{
		scene->Finalize();
		return fail("Default ActorImprint root is missing its required Transform.");
	}

	ActorImprintDefinitionExpansion expansion;
	expansion.root = root;
	expansion.actorGuids.emplace(1, root->GetGuid());
	expansion.components.emplace(2, transform);
	ActorImprintEditingObjectMap objectMap;
	if (!objectMap.Initialize(*scene, expansion, 3))
	{
		scene->Finalize();
		return fail("Could not initialize default ActorImprint LocalObjectIDs.");
	}

	auto result = Capture(*scene, objectMap, assets, outJson, outError);
	scene->Finalize();
	return result;
}

std::unique_ptr<const ActorImprint> ActorImprintEditingSnapshot::Capture(
	const SceneBase& scene, const ActorImprintEditingObjectMap& objectMap,
	const AssetManager& assets, nlohmann::json& outJson,
	ActorImprintEditingSnapshotError* outError)
{
	if (outError) *outError = {};
	auto fail = [&](ActorImprintAssetErrorCode code, LocalObjectId id,
		std::string path, std::string message) -> std::unique_ptr<const ActorImprint>
	{
		if (outError) *outError = { code, id, std::move(path), std::move(message) };
		return nullptr;
	};

	try
	{
		if (scene.GetStructurePolicy() != SceneStructurePolicy::SingleRootClosedSubtree ||
			!scene.GetImprintInstances().GetInstances().empty())
			return fail(ActorImprintAssetErrorCode::InvalidObjectGraph, 0, {},
				"ActorImprint Working Scene policy or provenance is invalid.");
		const auto roots = scene.GetRootActors();
		if (roots.size() != 1 || !roots.front() || roots.front()->IsDestroyed())
			return fail(ActorImprintAssetErrorCode::InvalidObjectGraph, 0, {},
				"ActorImprint Working Scene requires exactly one live root.");

		ActorImprintEditingObjectMap::Snapshot identities;
		if (!objectMap.CaptureSnapshot(scene, identities))
			return fail(ActorImprintAssetErrorCode::InvalidObjectGraph, 0, {},
				"Working Scene and LocalObjectID map are inconsistent.");
		const LocalObjectId rootId = objectMap.FindActor(roots.front()->GetGuid());
		if (rootId == InvalidLocalObjectId)
			return fail(ActorImprintAssetErrorCode::InvalidObjectGraph, 0, {}, "Root LocalObjectID is missing.");

		ActorImprintReferenceCodec::ActorGuids actorGuids;
		actorGuids.reserve(identities.actors.size());
		for (const auto& actor : identities.actors) actorGuids.emplace(actor.id, actor.guid);
		ActorImprintReferenceCodec actorCodec(std::move(actorGuids));
		SceneActorReferenceContext actorContext(scene);
		AssetReferenceCodec assetCodec;
		AssetManagerAssetReferenceContext assetContext(assets);
		ReflectionSaveContext save{
			.actorReferenceCodec = &actorCodec,
			.actorReferenceContext = &actorContext,
			.assetReferenceCodec = &assetCodec,
			.assetReferenceContext = &assetContext,
		};

		std::unordered_map<Guid, std::vector<const ActorImprintEditingObjectMap::ComponentEntry*>> componentsByActor;
		for (const auto& component : identities.components)
			componentsByActor[component.actorGuid].push_back(&component);

		nlohmann::json actors = nlohmann::json::array();
		for (std::size_t i = 0; i < identities.actors.size(); ++i)
		{
			const auto& identity = identities.actors[i];
			Actor* actor = scene.ResolveActor(identity.guid);
			if (!actor || actor->IsDestroyed())
				return fail(ActorImprintAssetErrorCode::InvalidObjectGraph, identity.id,
					"/actors/" + std::to_string(i), "Mapped Actor is not live.");

			nlohmann::json actorProperties;
			ReflectionError reflectionError;
			if (!ReflectionSerializer::Serialize(GetActorMetadata(), typeid(Actor), actor,
				actorProperties, save, &reflectionError))
				return fail(ActorImprintAssetErrorCode::InvalidProperty, identity.id,
					"/actors/" + std::to_string(i) + "/properties" +
					(reflectionError.path ? reflectionError.path->ToString() : std::string{}), reflectionError.message);

			LocalObjectId parentId = InvalidLocalObjectId;
			if (Actor* parent = actor->GetParent()) parentId = objectMap.FindActor(parent->GetGuid());
			if (actor != roots.front() && parentId == InvalidLocalObjectId)
				return fail(ActorImprintAssetErrorCode::InvalidObjectGraph, identity.id,
					"/actors/" + std::to_string(i) + "/parentLocalObjectId", "Parent is outside the Working Scene map.");

			nlohmann::json components = nlohmann::json::array();
			for (const auto* componentIdentity : componentsByActor[identity.guid])
			{
				const auto type = ComponentRegistry::Get().GetTypeId(componentIdentity->typeName);
				Component* component = type
					? actor->GetComponentByExactType(*type, componentIdentity->occurrenceIndex) : nullptr;
				const TypeMetadata* metadata = type ? ComponentRegistry::Get().GetMetadata(*type) : nullptr;
				if (!component || component->IsDestroyed() || !metadata)
					return fail(ActorImprintAssetErrorCode::InvalidComponent, componentIdentity->id,
						"/actors/" + std::to_string(i) + "/components", "Mapped Component is not live or registered.");
				nlohmann::json properties;
				ReflectionError componentError;
				if (!ReflectionSerializer::Serialize(*metadata, *type, component, properties, save, &componentError))
					return fail(ActorImprintAssetErrorCode::InvalidProperty, componentIdentity->id,
						"/actors/" + std::to_string(i) + "/components/properties" +
						(componentError.path ? componentError.path->ToString() : std::string{}), componentError.message);
				components.push_back({ { "localObjectId", componentIdentity->id },
					{ "type", componentIdentity->typeName }, { "properties", std::move(properties) } });
			}

			actors.push_back({ { "localObjectId", identity.id },
				{ "parentLocalObjectId", parentId == InvalidLocalObjectId ? nlohmann::json(nullptr) : nlohmann::json(parentId) },
				{ "properties", std::move(actorProperties) }, { "components", std::move(components) } });
		}

		nlohmann::json candidateJson{
			{ "version", ActorImprint::SCHEMA_VERSION },
			{ "definitionRevision", DefinitionRevision::Generate().ToString() },
			{ "rootActorLocalObjectId", rootId },
			{ "nextLocalObjectId", identities.nextLocalObjectId },
			{ "actors", std::move(actors) },
		};
		ActorImprintAssetError assetError;
		auto candidate = ActorImprintAssetDeserializer::Deserialize(candidateJson, &assetContext, &assetError);
		if (!candidate)
			return fail(assetError.code, 0, std::move(assetError.path), std::move(assetError.message));
		outJson = ActorImprintAssetSerializer::Serialize(*candidate);
		return candidate;
	}
	catch (const std::exception& error)
	{
		return fail(ActorImprintAssetErrorCode::InvalidProperty, 0, {}, error.what());
	}
}
