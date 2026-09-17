#include "ActorImprintAssetDeserializer.h"
#include "Engine/Core/Debug/Debug.h"
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

	// Validate that the given JSON object contains only the specified fields and that all required fields are present.
	bool ValidateFields(const json& object, std::initializer_list<const char*> fields)
	{
		// JSON object validation
		if (!object.is_object())
		{
			DBG("Expected an object.");
			return false;
		}

		// Check for unknown fields and missing required fields.
		for (auto member = object.begin(); member != object.end(); ++member)
		{
			if (std::find(fields.begin(), fields.end(), member.key()) == fields.end())
			{
				DBG("Unknown field: %s", member.key().c_str());
				return false;
			}
		}

		// Check if the required fields are present in the object.
		for (const char* field : fields)
		{
			if (!object.contains(field))
			{
				DBG("Required field is missing.");
				return false;
			}
		}

		return true;
	}

	class DetachedActorSaveContext final : public ActorReferenceSaveContext
	{
		bool Validate(const Guid& guid) const override
		{
			// The Imprint codec itself enforces membership in the complete local map.
			return guid.IsValid();
		}
	};

	class MissingAssetSaveContext final : public AssetReferenceSaveContext
	{
		bool Validate(const Guid&, AssetType) const override
		{
			DBG("Asset reference: AssetNotFound.");
			return false;
		}
	};

	bool Normalize(
		const TypeMetadata& metadata,
		std::type_index type,
		void* object,
		const json& source,
		json& normalized,
		const ReflectionRestoreContext& restore,
		const ReflectionSaveContext& save)
	{
		if (!ReflectionDeserializer::Deserialize(metadata, type, source, object, restore) ||
			!ReflectionSerializer::Serialize(metadata, type, object, normalized, save))
		{
			return false;
		}

		// A stable default is the basis for every subsequent Override comparison.
		json repeated;

		if (!ReflectionDeserializer::Deserialize(metadata, type, normalized, object, restore) ||
			!ReflectionSerializer::Serialize(metadata, type, object, repeated, save))
		{
			return false;
		}

		if (normalized != repeated)
		{
			DBG("Reflection normalization is not idempotent.");
			return false;
		}

		return true;
	}
}

std::unique_ptr<const ActorImprint> ActorImprintAssetDeserializer::Deserialize(
	const nlohmann::json& source,
	const AssetReferenceSaveContext* assetContext)
{
	try
	{
		// Validate the top-level fields and version.
		if (!ValidateFields(
				source, {"version", "definitionRevision", "rootActorLocalObjectId", "nextLocalObjectId", "actors"}))
		{
			return nullptr;
		}

		// Check the version validation
		if (!source["version"].is_number_integer() || source["version"] != ActorImprint::SCHEMA_VERSION)
		{
			DBG("ActorImprint asset version must be integer 1.");
			return nullptr;
		}

		// Check the revision validation
		DefinitionRevision revision;

		if (!source["definitionRevision"].is_string() ||
			!DefinitionRevision::TryParse(source["definitionRevision"].get<std::string>(), revision))
		{
			DBG("Expected a nonzero GUID revision.");
			return nullptr;
		}

		if (!ActorImprintDetail::ValidateObjectGraph(source))
		{
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
				DBG("Could not allocate validation identity.");
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

			if (!ValidateFields(actorJson, {"localObjectId", "parentLocalObjectId", "properties", "components"}))
			{
				return nullptr;
			}

			auto candidate = ActorFactory::RestoreActorShell({}, guids[i]);
			ActorImprint::ActorDefinition actor{actorJson["localObjectId"].get<LocalObjectId>(),
				actorJson["parentLocalObjectId"].is_null() ? 0 : actorJson["parentLocalObjectId"].get<LocalObjectId>(),
				{}, {}};

			if (!Normalize(GetActorMetadata(), typeid(Actor), candidate.get(), actorJson["properties"],
					actor.properties, restore, save))
			{
				return nullptr;
			}

			for (std::size_t j = 0; j < actorJson["components"].size(); ++j)
			{
				const json& componentJson = actorJson["components"][j];

				if (!ValidateFields(componentJson, {"localObjectId", "type", "properties"}))
				{
					return nullptr;
				}

				if (!componentJson["type"].is_string())
				{
					DBG("Expected a registered stable component name.");
					return nullptr;
				}

				const std::string name = componentJson["type"].get<std::string>();
				const auto type = registry.GetTypeId(name);
				const TypeMetadata* metadata = registry.GetMetadata(name);
				std::unique_ptr<Component> component(registry.Create(name));

				if (!type || !metadata || !component || typeid(*component) != *type ||
					!candidate->CanAddComponent(*type))
				{
					DBG("Component factory, metadata or cardinality/family policy is invalid.");
					return nullptr;
				}

				Component* owned = candidate->AddComponent(std::move(component));
				ActorImprint::ComponentDefinition definition{
					componentJson["localObjectId"].get<LocalObjectId>(), name, *type, {}};

				if (!owned || !Normalize(*metadata, *type, owned, componentJson["properties"], definition.properties,
								  restore, save))
				{
					return nullptr;
				}

				actor.components.push_back(std::move(definition));
			}

			if (!candidate->HasComponentFamily(ComponentFamily::Transform))
			{
				DBG("Actor requires one Transform-family component.");
				return nullptr;
			}

			std::sort(actor.components.begin(), actor.components.end(), [](const auto& a, const auto& b)
			{
				return a.id < b.id;
			});
			actors.push_back(std::move(actor));
		}

		std::sort(actors.begin(), actors.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
		return std::unique_ptr<const ActorImprint>(
			new ActorImprint(revision, source["rootActorLocalObjectId"].get<LocalObjectId>(),
				source["nextLocalObjectId"].get<LocalObjectId>(), std::move(actors)));
	}
	catch (const std::exception& exception)
	{
		DBG("%s", exception.what());
		return nullptr;
	}
}

std::unique_ptr<const ActorImprint> ActorImprintAssetDeserializer::Load(
	const std::string& path,
	const AssetReferenceSaveContext* assetContext)
{
	std::ifstream input(path, std::ios::binary);

	if (!input)
	{
		DBG("Could not open ActorImprint asset.");
		return nullptr;
	}

	try
	{
		bool duplicateKey = false;
		std::vector<std::unordered_set<std::string>> objects;
		auto callback = [&](int, json::parse_event_t event, json& value)
		{
			if (event == json::parse_event_t::object_start)
			{
				objects.emplace_back();
			}
			else if (event == json::parse_event_t::object_end)
			{
				objects.pop_back();
			}
			else if (event == json::parse_event_t::key && !objects.back().insert(value.get<std::string>()).second)
			{
				duplicateKey = true;
			}

			return true;
		};
		json source = json::parse(input, callback);

		if (input.bad() || duplicateKey)
		{
			DBG("Asset read failed or contained duplicate JSON fields.");
			return nullptr;
		}

		return Deserialize(source, assetContext);
	}
	catch (const std::exception& exception)
	{
		DBG("%s", exception.what());
		return nullptr;
	}
}
