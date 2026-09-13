#include "ActorImprintAssetDeserializer.h"
#include "ActorImprintReferenceCodec.h"
#include "Detail/ActorImprintObjectGraph.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorMetadata.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "Engine/Scene/ComponentRegistry.h"
#include <algorithm>
#include <fstream>
#include <unordered_set>

namespace
{
	using json = nlohmann::json;
	using ErrorCode = ActorImprintAssetErrorCode;

	bool Fail(ActorImprintAssetError* error, ErrorCode code, std::string path, std::string message)
	{
		if (error) *error = { code, std::move(path), std::move(message) };
		return false;
	}

	bool ValidateFields(const json& object, std::initializer_list<const char*> fields,
		const std::string& path, ActorImprintAssetError* error)
	{
		if (!object.is_object()) return Fail(error, ErrorCode::InvalidSchema, path, "Expected an object.");
		for (auto member = object.begin(); member != object.end(); ++member)
		{
			if (std::find(fields.begin(), fields.end(), member.key()) == fields.end())
			{
				return Fail(error, ErrorCode::InvalidSchema, (json::json_pointer(path) / member.key()).to_string(),
					"Unknown field: " + member.key());
			}
		}
		for (const char* field : fields)
		{
			if (!object.contains(field))
			{
				return Fail(error, ErrorCode::InvalidSchema, path + '/' + field, "Required field is missing.");
			}
		}
		return true;
	}

	class DetachedActorSaveContext final : public ActorReferenceSaveContext
	{
		ActorReferenceCodecResult Validate(const Guid& guid) const override
		{
			// The Imprint codec itself enforces membership in the complete local map.
			return guid.IsValid() ? ActorReferenceCodecResult::Success : ActorReferenceCodecResult::InvalidGuid;
		}
	};

	class MissingAssetSaveContext final : public AssetReferenceSaveContext
	{
		AssetReferenceCodecResult Validate(const Guid&, AssetType) const override
		{
			return AssetReferenceCodecResult::AssetNotFound;
		}
	};

	bool Normalize(const TypeMetadata& metadata, std::type_index type, void* object,
		const json& source, json& normalized, const ReflectionRestoreContext& restore,
		const ReflectionSaveContext& save, const std::string& path, ActorImprintAssetError* error)
	{
		ReflectionError reflectionError;
		if (!ReflectionDeserializer::Deserialize(metadata, type, source, object, restore, &reflectionError) ||
			!ReflectionSerializer::Serialize(metadata, type, object, normalized, save, &reflectionError))
		{
			return Fail(error, ErrorCode::InvalidProperty,
				path + (reflectionError.path ? reflectionError.path->ToString() : ""), reflectionError.message);
		}

		// A stable default is the basis for every subsequent Override comparison.
		json repeated;
		if (!ReflectionDeserializer::Deserialize(metadata, type, normalized, object, restore, &reflectionError) ||
			!ReflectionSerializer::Serialize(metadata, type, object, repeated, save, &reflectionError))
		{
			return Fail(error, ErrorCode::InvalidProperty,
				path + (reflectionError.path ? reflectionError.path->ToString() : ""), reflectionError.message);
		}
		if (normalized != repeated)
		{
			return Fail(error, ErrorCode::InvalidProperty, path, "Reflection normalization is not idempotent.");
		}
		return true;
	}
}

std::unique_ptr<const ActorImprint> ActorImprintAssetDeserializer::Deserialize(
	const nlohmann::json& source, const AssetReferenceSaveContext* assetContext, ActorImprintAssetError* outError)
{
	if (outError) *outError = {};
	try
	{
		if (!ValidateFields(source, { "version", "definitionRevision", "rootActorLocalObjectId", "nextLocalObjectId", "actors" },
			"", outError)) return nullptr;
		if (!source["version"].is_number_integer() || source["version"] != ActorImprint::SCHEMA_VERSION)
		{
			Fail(outError, ErrorCode::UnsupportedVersion, "/version", "ActorImprint asset version must be integer 1.");
			return nullptr;
		}
		DefinitionRevision revision;
		if (!source["definitionRevision"].is_string() ||
			!DefinitionRevision::TryParse(source["definitionRevision"].get<std::string>(), revision))
		{
			Fail(outError, ErrorCode::InvalidRevision, "/definitionRevision", "Expected a nonzero GUID revision.");
			return nullptr;
		}
		ActorImprintDetail::ObjectGraphError graphError;
		if (!ActorImprintDetail::ValidateObjectGraph(source, graphError))
		{
			Fail(outError, ErrorCode::InvalidObjectGraph, graphError.path, graphError.message);
			return nullptr;
		}

		// Only temporary GUIDs are needed to pass local references through the
		// existing Reflection value/setter path. No Scene or runtime pool is created.
		std::vector<Guid> guids;
		ActorImprintReferenceCodec::ActorGuids localActors;
		for (const json& actor : source["actors"])
		{
			guids.push_back(GuidGenerator::Generate());
			if (!guids.back().IsValid())
			{
				Fail(outError, ErrorCode::InvalidObjectGraph, "/actors", "Could not allocate validation identity.");
				return nullptr;
			}
			localActors.emplace(actor["localObjectId"].get<LocalObjectId>(), guids.back());
		}
		ActorImprintReferenceCodec actorCodec(std::move(localActors));
		DetachedActorSaveContext actorContext;
		AssetReferenceCodec assetCodec;
		MissingAssetSaveContext missingAssetContext;
		ReflectionSaveContext save{ &actorCodec, &actorContext, &assetCodec,
			assetContext ? assetContext : &missingAssetContext };
		ReflectionRestoreContext restore{ .actorReferenceCodec = &actorCodec, .assetReferenceCodec = &assetCodec };
		ComponentRegistry& registry = ComponentRegistry::Get();
		std::vector<ActorImprint::ActorDefinition> actors;

		for (std::size_t i = 0; i < source["actors"].size(); ++i)
		{
			const json& actorJson = source["actors"][i];
			const std::string actorPath = "/actors/" + std::to_string(i);
			if (!ValidateFields(actorJson, { "localObjectId", "parentLocalObjectId", "properties", "components" },
				actorPath, outError)) return nullptr;
			auto candidate = ActorFactory::RestoreActorShell({}, guids[i]);
			ActorImprint::ActorDefinition actor{
				actorJson["localObjectId"].get<LocalObjectId>(),
				actorJson["parentLocalObjectId"].is_null() ? 0 : actorJson["parentLocalObjectId"].get<LocalObjectId>(), {}, {} };
			if (!Normalize(GetActorMetadata(), typeid(Actor), candidate.get(), actorJson["properties"], actor.properties,
				restore, save, actorPath + "/properties", outError)) return nullptr;

			for (std::size_t j = 0; j < actorJson["components"].size(); ++j)
			{
				const json& componentJson = actorJson["components"][j];
				const std::string path = actorPath + "/components/" + std::to_string(j);
				if (!ValidateFields(componentJson, { "localObjectId", "type", "properties" }, path, outError)) return nullptr;
				if (!componentJson["type"].is_string())
				{
					Fail(outError, ErrorCode::InvalidComponent, path + "/type", "Expected a registered stable component name.");
					return nullptr;
				}
				const std::string name = componentJson["type"].get<std::string>();
				const auto type = registry.GetTypeId(name);
				const TypeMetadata* metadata = registry.GetMetadata(name);
				std::unique_ptr<Component> component(registry.Create(name));
				if (!type || !metadata || !component || typeid(*component) != *type || !candidate->CanAddComponent(*type))
				{
					Fail(outError, ErrorCode::InvalidComponent, path + "/type", "Component factory, metadata or cardinality/family policy is invalid.");
					return nullptr;
				}
				Component* owned = candidate->AddComponent(std::move(component));
				ActorImprint::ComponentDefinition definition{ componentJson["localObjectId"].get<LocalObjectId>(), name, *type, {} };
				if (!owned || !Normalize(*metadata, *type, owned, componentJson["properties"], definition.properties,
					restore, save, path + "/properties", outError)) return nullptr;
				actor.components.push_back(std::move(definition));
			}
			if (!candidate->HasComponentFamily(ComponentFamily::Transform))
			{
				Fail(outError, ErrorCode::InvalidComponent, actorPath + "/components", "Actor requires one Transform-family component.");
				return nullptr;
			}
			std::sort(actor.components.begin(), actor.components.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
			actors.push_back(std::move(actor));
		}
		std::sort(actors.begin(), actors.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
		return std::unique_ptr<const ActorImprint>(new ActorImprint(revision,
			source["rootActorLocalObjectId"].get<LocalObjectId>(), source["nextLocalObjectId"].get<LocalObjectId>(), std::move(actors)));
	}
	catch (const std::exception& exception)
	{
		Fail(outError, ErrorCode::InvalidProperty, "", exception.what());
		return nullptr;
	}
}

std::unique_ptr<const ActorImprint> ActorImprintAssetDeserializer::Load(
	const std::string& path, const AssetReferenceSaveContext* assetContext, ActorImprintAssetError* outError)
{
	if (outError) *outError = {};
	std::ifstream input(path, std::ios::binary);
	if (!input)
	{
		Fail(outError, ErrorCode::IoError, path, "Could not open ActorImprint asset.");
		return nullptr;
	}
	try
	{
		// DOM input cannot retain duplicate keys. Detect them while reading a file.
		bool duplicateKey = false;
		std::vector<std::unordered_set<std::string>> objects;
		auto callback = [&](int, json::parse_event_t event, json& value)
		{
			if (event == json::parse_event_t::object_start) objects.emplace_back();
			else if (event == json::parse_event_t::object_end) objects.pop_back();
			else if (event == json::parse_event_t::key && !objects.back().insert(value.get<std::string>()).second) duplicateKey = true;
			return true;
		};
		json source = json::parse(input, callback);
		if (input.bad() || duplicateKey)
		{
			Fail(outError, ErrorCode::InvalidJson, path, "Asset read failed or contained duplicate JSON fields.");
			return nullptr;
		}
		return Deserialize(source, assetContext, outError);
	}
	catch (const std::exception& exception)
	{
		Fail(outError, ErrorCode::InvalidJson, path, exception.what());
		return nullptr;
	}
}
