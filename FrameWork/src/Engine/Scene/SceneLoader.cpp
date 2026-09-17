#include "SceneLoader.h"
#include "SceneVersion.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/ActorImprint/ActorImprint.h"
#include "Engine/ActorImprint/ActorImprintInstanceDeserializer.h"
#include "Engine/ActorImprint/ActorImprintInstanceRecordCodec.h"
#include "Engine/ActorImprint/ActorImprintInstanceRegistry.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/SkyRenderer.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/ComponentReflection.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Graphics/LightTypes.h"
#include "Engine/Graphics/RenderSystem.h"
#include "Engine/Scene/ActorDeserializer.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <utility>

using json = nlohmann::json;

namespace
{
	bool Fail(
		SceneLoadError& error,
		SceneLoadErrorCode code,
		const std::string& path,
		const std::string& message)
	{
		error.code = code;
		error.path = path;
		error.message = message;
		return false;
	}

	std::string AppendPath(const std::string& base, const std::string& member)
	{
		return (json::json_pointer(base) / member).to_string();
	}

	std::string ActorPath(std::size_t index)
	{
		return "/actors/" + std::to_string(index);
	}

	std::string InstancePath(std::size_t index)
	{
		return "/actorImprintInstances/" + std::to_string(index);
	}

	bool Contains(std::initializer_list<std::string_view> fields, std::string_view field)
	{
		return std::find(fields.begin(), fields.end(), field) != fields.end();
	}

	bool ValidateFields(
		const json& object,
		std::initializer_list<std::string_view> required,
		std::initializer_list<std::string_view> optional,
		const std::string& path,
		SceneLoadError& error)
	{
		if (!object.is_object())
		{
			return Fail(error, SceneLoadErrorCode::InvalidSchema, path, "Expected an object.");
		}

		for (auto member = object.begin(); member != object.end(); ++member)
		{
			if (!Contains(required, member.key()) && !Contains(optional, member.key()))
			{
				return Fail(error, SceneLoadErrorCode::InvalidSchema,
					AppendPath(path, member.key()), "Unknown field: " + member.key());
			}
		}

		for (std::string_view field : required)
		{
			if (!object.contains(field))
			{
				return Fail(error, SceneLoadErrorCode::InvalidSchema,
					AppendPath(path, std::string(field)), "Required field is missing.");
			}
		}

		return true;
	}

	bool ReadGuid(const json& value, Guid& outGuid)
	{
		if (!value.is_string())
		{
			return false;
		}

		const std::string text = value.get<std::string>();
		return text.find('\0') == std::string::npos && Guid::TryParse(text, outGuid) && outGuid.IsValid();
	}

	std::string GuidSortKey(const Guid& guid)
	{
		return guid.ToString();
	}

	bool FitsFiniteFloat(double value)
	{
		return std::isfinite(value) &&
			value >= static_cast<double>(std::numeric_limits<float>::lowest()) &&
			value <= static_cast<double>(std::numeric_limits<float>::max());
	}

	bool ValidateVector3(const json& value, const std::string& path, SceneLoadError& error)
	{
		if (!value.is_array() || value.size() != 3)
		{
			return Fail(error, SceneLoadErrorCode::InvalidSceneSettings, path,
				"Expected an array containing exactly three finite numbers.");
		}

		for (std::size_t index = 0; index < value.size(); ++index)
		{
			if (!value[index].is_number())
			{
				return Fail(error, SceneLoadErrorCode::InvalidSceneSettings, path + "/" + std::to_string(index),
					"Expected a finite number.");
			}

			const double source = value[index].get<double>();

			if (!FitsFiniteFloat(source))
			{
				return Fail(error, SceneLoadErrorCode::InvalidSceneSettings,
					path + "/" + std::to_string(index), "Number is outside the finite float range.");
			}
		}

		return true;
	}
}

SceneLoadResult SceneLoader::LoadCandidate(const std::string& filePath, EngineContext& context)
{
	return LoadCandidate(filePath, context, {});
}

SceneLoadResult SceneLoader::LoadCandidate(
	const std::string& filePath,
	EngineContext& context,
	SceneLoadOptions options)
{
	SceneLoadError error;
	error.assetPath = filePath;

	if (filePath.empty())
	{
		Fail(
			error,
			SceneLoadErrorCode::InvalidArgument,
			{},
			"Scene asset path must not be empty.");

		return { nullptr, std::move(error) };
	}

	std::string fullPath;

	try
	{
		fullPath = PathManager::Resolve(filePath);
	}
	catch (const std::exception& exception)
	{
		Fail(
			error,
			SceneLoadErrorCode::FileOpenFailed,
			{},
			exception.what());

		return { nullptr, std::move(error) };
	}

	std::ifstream file(fullPath);

	if (!file.is_open())
	{
		Fail(
			error,
			SceneLoadErrorCode::FileOpenFailed,
			{},
			"Failed to open Scene asset: " + fullPath);

		return { nullptr, std::move(error) };
	}

	json sceneRecord;

	try
	{
		bool duplicateField = false;
		std::string duplicateFieldName;
		std::vector<std::unordered_set<std::string>> objectFields;

		auto callback =
			[&](int, json::parse_event_t event, json& value)
			{
				if (event == json::parse_event_t::object_start)
				{
					objectFields.emplace_back();
				}

				else if (event == json::parse_event_t::object_end)
				{
					objectFields.pop_back();
				}
				else if (event == json::parse_event_t::key &&
					!objectFields.back().insert(value.get<std::string>()).second)
				{
					duplicateField = true;
					duplicateFieldName = value.get<std::string>();
				}

				return true;
			};

		sceneRecord = json::parse(file, callback);

		if (file.bad() || duplicateField)
		{
			std::string message = duplicateField
				? "Scene contains a duplicate JSON field: " + duplicateFieldName
				: "Scene file read failed while parsing JSON.";

			Fail(error, SceneLoadErrorCode::JsonParseFailed, {}, message);

			return { nullptr, std::move(error) };
		}
	}
	catch (const json::exception& exception)
	{
		Fail(error, SceneLoadErrorCode::JsonParseFailed, {}, exception.what());

		return { nullptr, std::move(error) };
	}

	return LoadCandidate(
		sceneRecord,
		context,
		filePath,
		options);
}

SceneLoadResult SceneLoader::LoadCandidate(
	const json& sceneRecord,
	EngineContext& context,
	std::string assetPath)

{
	return LoadCandidate(sceneRecord, context, std::move(assetPath), {});
}

SceneLoadResult SceneLoader::LoadCandidate(
	const json& sceneRecord,
	EngineContext& context,
	std::string assetPath,
	SceneLoadOptions options)
{
	return LoadCandidateImpl(sceneRecord, context, std::move(assetPath), true, std::move(options));
}

SceneLoadResult SceneLoader::LoadPreparedCandidate(
	const json& sceneRecord,
	EngineContext& context,
	std::string assetPath)
{
	return LoadCandidateImpl(sceneRecord, context, std::move(assetPath), false, {});
}

SceneLoadResult SceneLoader::LoadCandidateImpl(
	const json& sceneRecord,
	EngineContext& context,
	std::string assetPath,
	bool publish,
	SceneLoadOptions options)
{
	SceneLoadError error;
	error.assetPath = std::move(assetPath);

	if (!sceneRecord.is_object())
	{
		Fail(error, SceneLoadErrorCode::InvalidSchema, {}, "Scene record must be an object.");
		return { nullptr, std::move(error) };
	}

	if (!sceneRecord.contains("version"))
	{
		Fail(error, SceneLoadErrorCode::InvalidSchema, "/version", "Required field is missing.");
		return { nullptr, std::move(error) };
	}

	if (!sceneRecord["version"].is_number_integer())
	{
		Fail(error, SceneLoadErrorCode::InvalidSchema, "/version", "Scene version must be an integer.");
		return { nullptr, std::move(error) };
	}

	const json& versionValue = sceneRecord["version"];
	bool strictV4 = false;
	bool compatibleV3 = false;
	bool legacyV2 = false;

	if (versionValue.is_number_unsigned())
	{
		const std::uint64_t version = versionValue.get<std::uint64_t>();
		strictV4 = version == static_cast<std::uint64_t>(CURRENT_SCENE_VERSION);
		compatibleV3 = version == static_cast<std::uint64_t>(COMPATIBLE_SCENE_VERSION);
		legacyV2 = version == 2;
	}
	else
	{
		const std::int64_t version = versionValue.get<std::int64_t>();
		strictV4 = version == CURRENT_SCENE_VERSION;
		compatibleV3 = version == COMPATIBLE_SCENE_VERSION;
		legacyV2 = version == 2;
	}

	if (!strictV4 && !compatibleV3)
	{
		const std::string message = legacyV2
			? "Scene version 2 is no longer supported; migrate it to version 3 before loading."
			: "Unsupported Scene version: " + versionValue.dump();
		Fail(error, SceneLoadErrorCode::UnsupportedVersion, "/version", message);
		return { nullptr, std::move(error) };
	}

	if (compatibleV3)
	{
		const auto instances = sceneRecord.find("actorImprintInstances");

		if (instances != sceneRecord.end() && (!instances->is_array() || !instances->empty()))
		{
			Fail(error, SceneLoadErrorCode::InvalidSchema, "/actorImprintInstances",
				"Scene version 3 cannot contain ActorImprint Instance records; save it as version 4.");
			return { nullptr, std::move(error) };
		}
	}

	if (strictV4 && !ValidateFields(sceneRecord,
		{ "version", "directional_light", "actors", "actorImprintInstances" }, {}, {}, error))
	{
		return { nullptr, std::move(error) };
	}

	std::unordered_map<Guid, ActorOrigin> origins;
	std::vector<ActorLoadRecord> actorRecords;
	std::vector<InstanceLoadRecord> instanceRecords;

	if (!BuildActorLoadRecords(sceneRecord, strictV4, actorRecords, origins, error) ||
		!ValidateParentReferences(actorRecords, error) ||
		!ValidateHierarchyCycles(actorRecords, error) ||
		(strictV4 && !BuildInstanceLoadRecords(sceneRecord, instanceRecords, origins, error)) ||
		!ValidateSceneSettings(sceneRecord, strictV4, error))
	{
		return { nullptr, std::move(error) };
	}

	std::unordered_set<Guid> reservedSceneGuids;
	reservedSceneGuids.reserve(origins.size());

	for (const auto& [guid, origin] : origins)
	{
		(void)origin;
		reservedSceneGuids.insert(guid);
	}

	auto candidate = std::make_unique<SceneBase>();
	candidate->Initialize(context);
	candidate->m_unpublishedCandidate = true;

	auto Abort = [&]() -> SceneLoadResult
	{
		DBG("SceneLoader: Candidate load failed at '%s': %s",
			error.path.c_str(), error.message.c_str());
		candidate->DiscardUnpublishedCandidate();
		return { nullptr, std::move(error) };
	};

	try
	{
		if (!RestoreOrdinaryActors(actorRecords, *candidate, options, error) ||
			!RestoreInstances(instanceRecords, reservedSceneGuids, origins, *candidate, error) ||
			!RestoreOrdinaryHierarchy(actorRecords, *candidate, error) ||
			!RestoreComponentReferences(origins, *candidate, error))
		{
			return Abort();
		}

		Actor* invalidUIActor = nullptr;

		if (!candidate->ApplyAllUIHierarchyConstraints(&invalidUIActor))
		{
			const auto origin = invalidUIActor ? origins.find(invalidUIActor->GetGuid()) : origins.end();
			const std::string path = origin == origins.end() ? "/actors" : origin->second.path;
			Fail(error, SceneLoadErrorCode::UIHierarchyFailed, path,
				"Scene UI hierarchy is incompatible with the Transform-family component on Actor '" +
				(invalidUIActor ? invalidUIActor->GetName() : std::string("<unknown>")) + "'.");
			return Abort();
		}

		if (!ApplySceneSettings(sceneRecord, *candidate, error) ||
			!ValidateInstanceRegistry(*candidate, error))
		{
			return Abort();
		}

		// Preallocate final component storage and the traversal used by commit while
		// the candidate can still be discarded without lifecycle callbacks.
		const auto& commitActors = candidate->PrepareUnpublishedCandidateForCommit();
		ConfigureMainCamera(*candidate, commitActors);
		ConfigureInitialSky(*candidate, commitActors);
	}
	catch (const std::exception& exception)
	{
		Fail(error, SceneLoadErrorCode::InvalidSchema, error.path, exception.what());
		return Abort();
	}
	catch (...)
	{
		Fail(error, SceneLoadErrorCode::InvalidSchema, error.path,
			"Scene candidate construction threw an unknown exception.");
		return Abort();
	}

	// No recoverable persisted-graph work remains. Normal callers publish now;
	// ET-14 retains the prepared candidate until every live Scene validates.
	if (publish)
	{
		candidate->PublishUnpublishedCandidate();
	}

	return { std::move(candidate), {} };
}

bool SceneLoader::BuildActorLoadRecords(
	const json& sceneJson,
	bool strictV4,
	std::vector<ActorLoadRecord>& outRecords,
	std::unordered_map<Guid, ActorOrigin>& origins,
	SceneLoadError& error)
{
	if (!sceneJson.contains("actors"))
	{
		return Fail(error, SceneLoadErrorCode::InvalidSchema, "/actors", "Required field is missing.");
	}

	if (!sceneJson["actors"].is_array())
	{
		return Fail(error, SceneLoadErrorCode::InvalidSchema, "/actors", "Scene actors must be an array.");
	}

	const json& actors = sceneJson["actors"];
	outRecords.reserve(actors.size());

	for (std::size_t actorIndex = 0; actorIndex < actors.size(); ++actorIndex)
	{
		const json& actorJson = actors[actorIndex];
		const std::string path = ActorPath(actorIndex);

		if (!actorJson.is_object())
		{
			return Fail(error, SceneLoadErrorCode::InvalidSchema, path, "Actor record must be an object.");
		}

		if (strictV4)
		{
			if (!ValidateFields(actorJson,
					{"actorId", "parentId", "name", "is_active", "tag", "components"}, {}, path, error))
			{
				return false;
			}

			if (!actorJson["name"].is_string())
			{
				return Fail(error, SceneLoadErrorCode::InvalidSchema, path + "/name", "Actor name must be a string.");
			}

			if (!actorJson["is_active"].is_boolean())
			{
				return Fail(error, SceneLoadErrorCode::InvalidSchema, path + "/is_active",
					"Actor active state must be a boolean.");
			}

			if (!actorJson["tag"].is_string())
			{
				return Fail(error, SceneLoadErrorCode::InvalidSchema, path + "/tag", "Actor tag must be a string.");
			}

			if (!actorJson["components"].is_array())
			{
				return Fail(error, SceneLoadErrorCode::InvalidSchema, path + "/components",
					"Actor components must be an array.");
			}

			for (std::size_t componentIndex = 0; componentIndex < actorJson["components"].size(); ++componentIndex)
			{
				const json& component = actorJson["components"][componentIndex];
				const std::string componentPath = path + "/components/" + std::to_string(componentIndex);

				if (!ValidateFields(component, {"type", "data"}, {}, componentPath, error))
				{
					return false;
				}

				if (!component["type"].is_string() || component["type"].get<std::string>().empty())
				{
					return Fail(error, SceneLoadErrorCode::InvalidSchema, componentPath + "/type",
						"Component type must be a nonempty string.");
				}

				if (!component["data"].is_object())
				{
					return Fail(error, SceneLoadErrorCode::InvalidSchema, componentPath + "/data",
						"Component data must be an object.");
				}
			}
		}

		if (!actorJson.contains("actorId"))
		{
			return Fail(error, SceneLoadErrorCode::InvalidSchema, path + "/actorId", "Required field is missing.");
		}

		Guid actorGuid;

		if (!ReadGuid(actorJson["actorId"], actorGuid))
		{
			return Fail(error, SceneLoadErrorCode::InvalidActorGuid, path + "/actorId",
				"Actor ID must be a nonzero GUID string.");
		}

		if (!origins.emplace(actorGuid, ActorOrigin{ActorProvenance::Ordinary, path}).second)
		{
			return Fail(error, SceneLoadErrorCode::DuplicateActorGuid, path + "/actorId",
				"Actor GUID is duplicated across the Scene.");
		}

		ActorLoadRecord record;
		record.actorJson = &actorJson;
		record.actorGuid = actorGuid;
		record.sourceIndex = actorIndex;

		if (!actorJson.contains("parentId"))
		{
			return Fail(error, SceneLoadErrorCode::InvalidSchema, path + "/parentId", "Required field is missing.");
		}

		if (actorJson["parentId"].is_null())
		{
			record.hasParent = false;
		}
		else
		{
			Guid parentGuid;

			if (!ReadGuid(actorJson["parentId"], parentGuid))
			{
				return Fail(error, SceneLoadErrorCode::InvalidHierarchy, path + "/parentId",
					"Parent ID must be null or a nonzero GUID string.");
			}

			if (parentGuid == actorGuid)
			{
				return Fail(
					error, SceneLoadErrorCode::InvalidHierarchy, path + "/parentId", "Actor cannot be its own parent.");
			}

			record.hasParent = true;
			record.parentGuid = parentGuid;
		}

		outRecords.push_back(record);
	}

	std::sort(outRecords.begin(), outRecords.end(), [](const auto& left, const auto& right)
	{
		return GuidSortKey(left.actorGuid) < GuidSortKey(right.actorGuid);
	});

	return true;
}

bool SceneLoader::BuildInstanceLoadRecords(
	const json& sceneJson,
	std::vector<InstanceLoadRecord>& outRecords,
	std::unordered_map<Guid, ActorOrigin>& origins,
	SceneLoadError& error)
{
	const json& instances = sceneJson["actorImprintInstances"];

	if (!instances.is_array())
	{
		return Fail(error, SceneLoadErrorCode::InvalidSchema, "/actorImprintInstances",
			"ActorImprint Instances must be an array.");
	}

	outRecords.reserve(instances.size());

	for (std::size_t instanceIndex = 0; instanceIndex < instances.size(); ++instanceIndex)
	{
		const std::string path = InstancePath(instanceIndex);
		ActorImprintSerializedInstanceRecord record;

		if (!ActorImprintInstanceRecordReader::Read(instances[instanceIndex], record))
		{
			return Fail(error, SceneLoadErrorCode::InstanceDeserializationFailed,
				path, "ActorImprint Instance record is invalid.");
		}

		const json& actorGuids = instances[instanceIndex]["actorGuids"];

		for (std::size_t mappingIndex = 0; mappingIndex < actorGuids.size(); ++mappingIndex)
		{
			Guid actorGuid;
			ReadGuid(actorGuids[mappingIndex]["actorGuid"], actorGuid);

			const ActorProvenance provenance =
				actorGuid == record.rootActorGuid ? ActorProvenance::ImprintRoot : ActorProvenance::ImprintMember;
			const std::string mappingPath = path + "/actorGuids/" + std::to_string(mappingIndex) + "/actorGuid";

			if (!origins.emplace(actorGuid, ActorOrigin{provenance, path}).second)
			{
				return Fail(error, SceneLoadErrorCode::DuplicateActorGuid, mappingPath,
					"Actor GUID is duplicated across the Scene.");
			}
		}

		outRecords.push_back({ std::move(record), instanceIndex });
	}

	for (const InstanceLoadRecord& instance : outRecords)
	{
		if (!instance.record.externalParentActorGuid)
		{
			continue;
		}

		const auto parent = origins.find(*instance.record.externalParentActorGuid);

		if (parent == origins.end() || parent->second.provenance != ActorProvenance::Ordinary)
		{
			return Fail(error, SceneLoadErrorCode::InvalidHierarchy,
				InstancePath(instance.sourceIndex) + "/externalParentActorGuid",
				"External parent must identify an ordinary Actor in this Scene.");
		}
	}

	std::sort(outRecords.begin(), outRecords.end(), [](const auto& left, const auto& right)
	{
		return GuidSortKey(left.record.rootActorGuid) < GuidSortKey(right.record.rootActorGuid);
	});

	return true;
}

bool SceneLoader::ValidateParentReferences(
	const std::vector<ActorLoadRecord>& records,
	SceneLoadError& error)
{
	std::unordered_set<Guid> actorGuids;
	actorGuids.reserve(records.size());

	for (const ActorLoadRecord& record : records)
	{
		actorGuids.insert(record.actorGuid);
	}

	for (const ActorLoadRecord& record : records)
	{
		if (record.hasParent && !actorGuids.contains(record.parentGuid))
		{
			return Fail(error, SceneLoadErrorCode::InvalidHierarchy,
				ActorPath(record.sourceIndex) + "/parentId",
				"Ordinary Actor parent does not exist in the ordinary Actor array.");
		}
	}

	return true;
}

bool SceneLoader::ValidateHierarchyCycles(
	const std::vector<ActorLoadRecord>& records,
	SceneLoadError& error)
{
	std::unordered_map<Guid, Guid> parentMap;

	for (const ActorLoadRecord& record : records)
	{
		if (record.hasParent)
		{
			parentMap.emplace(record.actorGuid, record.parentGuid);
		}
	}

	std::unordered_map<Guid, VisitState> states;

	for (const ActorLoadRecord& record : records)
	{
		if (states[record.actorGuid] == VisitState::Visited)
		{
			continue;
		}

		std::vector<Guid> path;
		Guid current = record.actorGuid;

		for (;;)
		{
			VisitState& state = states[current];

			if (state == VisitState::Visiting)
			{
				return Fail(error, SceneLoadErrorCode::InvalidHierarchy, ActorPath(record.sourceIndex) + "/parentId",
					"Actor hierarchy contains a cycle.");
			}

			if (state == VisitState::Visited)
			{
				break;
			}

			state = VisitState::Visiting;
			path.push_back(current);
			const auto parent = parentMap.find(current);

			if (parent == parentMap.end())
			{
				break;
			}

			current = parent->second;
		}

		for (const Guid& visited : path)
		{
			states[visited] = VisitState::Visited;
		}
	}

	return true;
}

bool SceneLoader::RestoreOrdinaryActors(
	const std::vector<ActorLoadRecord>& records,
	SceneBase& scene,
	SceneLoadOptions options,
	SceneLoadError& error)
{
	for (const ActorLoadRecord& record : records)
	{
		ActorDeserializationError actorError;
		ActorDeserializationOptions actorOptions;
		actorOptions.unknownComponentPropertyPolicy = options.unknownComponentPropertyPolicy;
		auto actor = ActorDeserializer::DeserializeActorRecord(
			*record.actorJson, record.actorGuid, actorOptions, &actorError);

		if (!actor)
		{
			const std::string failureMessage = actorError.message.empty()
				? "Actor deserialization failed."
				: actorError.message;
			return Fail(error, SceneLoadErrorCode::ActorDeserializationFailed,
				ActorPath(record.sourceIndex) + actorError.path,
				failureMessage);
		}

		if (!scene.RegisterRestoredActor(std::move(actor)))
		{
			return Fail(error, SceneLoadErrorCode::ActorDeserializationFailed,
				ActorPath(record.sourceIndex), "Candidate Scene rejected the restored Actor.");
		}
	}

	return true;
}

bool SceneLoader::RestoreOrdinaryHierarchy(
	const std::vector<ActorLoadRecord>& records,
	SceneBase& scene,
	SceneLoadError& error)
{
	for (const ActorLoadRecord& record : records)
	{
		if (!record.hasParent)
		{
			continue;
		}

		Actor* child = scene.ResolveActor(record.actorGuid);
		Actor* parent = scene.ResolveActor(record.parentGuid);

		if (!child || !parent || !scene.RestoreParentRelationship(child, parent))
		{
			return Fail(error, SceneLoadErrorCode::InvalidHierarchy,
				ActorPath(record.sourceIndex) + "/parentId",
				"Failed to restore the ordinary Actor parent relationship.");
		}
	}

	return true;
}

bool SceneLoader::RestoreInstances(
	const std::vector<InstanceLoadRecord>& records,
	const std::unordered_set<Guid>& reservedSceneGuids,
	std::unordered_map<Guid, ActorOrigin>& origins,
	SceneBase& scene,
	SceneLoadError& error)
{
	if (records.empty())
	{
		return true;
	}

	EngineContext* context = scene.GetEngineContext();

	if (!context || !context->pActorImprintSystem || !context->pAssetManager)
	{
		return Fail(error, SceneLoadErrorCode::InstanceDeserializationFailed,
			"/actorImprintInstances", "Scene context has no ActorImprintSystem or AssetManager.");
	}

	ActorImprintSystem& system = *context->pActorImprintSystem;

	for (const InstanceLoadRecord& instance : records)
	{
		const std::string path = InstancePath(instance.sourceIndex);
		ActorImprintPreparedRestore prepared;

		if (!ActorImprintInstanceDeserializer::Prepare(
				instance.record, scene, system, prepared))
		{
			return Fail(error, SceneLoadErrorCode::InstanceDeserializationFailed,
				path, "ActorImprint Instance preparation failed.");
		}

		const ActorImprint* definition = system.ResolveForSceneCandidate(prepared.imprint);

		if (!definition)
		{
			return Fail(error, SceneLoadErrorCode::InstanceDeserializationFailed,
				path + "/assetGuid", "ActorImprint definition was not retained after loading.");
		}

		const auto rootMapping = prepared.input.actorGuids.find(definition->GetRootActorId());

		if (rootMapping == prepared.input.actorGuids.end() ||
			rootMapping->second != instance.record.rootActorGuid)
		{
			return Fail(error, SceneLoadErrorCode::InstanceDeserializationFailed,
				path + "/rootActorGuid",
				"Root Actor GUID does not match the definition root LocalObjectID mapping.");
		}

		Actor* root =
			system.RestoreInstanceForSceneCandidate(scene, prepared.imprint, prepared.input, reservedSceneGuids);

		if (!root)
		{
			return Fail(error, SceneLoadErrorCode::InstanceMaterializationFailed,
				path, "ActorImprint Instance materialization failed.");
		}

		const ActorImprintInstanceRecord* runtime = scene.GetImprintInstances().FindInstance(root->GetHandle());

		if (!runtime)
		{
			return Fail(error, SceneLoadErrorCode::InvalidInstanceRegistry, path,
				"Materialized Instance has no Scene registry record.");
		}

		for (const auto& [localId, identity] : runtime->actors)
		{
			const ActorProvenance provenance = localId == runtime->rootId
				? ActorProvenance::ImprintRoot : ActorProvenance::ImprintMember;
			origins.emplace(identity.guid, ActorOrigin{ provenance, path });
		}
	}

	return true;
}

bool SceneLoader::RestoreComponentReferences(
	const std::unordered_map<Guid, ActorOrigin>& origins,
	SceneBase& scene,
	SceneLoadError& error)
{
	SceneBase::StructuralMutationScope mutationScope(&scene);

	for (Actor* actor : scene.GetAllActors())
	{
		if (!actor)
		{
			return Fail(error, SceneLoadErrorCode::ReferenceResolutionFailed, "/actors",
				"Candidate Scene contains a null Actor.");
		}

		const auto origin = origins.find(actor->GetGuid());
		const std::string actorPath = origin == origins.end() ? std::string("/actors") : origin->second.path;
		const bool ordinary = origin != origins.end() && origin->second.provenance == ActorProvenance::Ordinary;
		const auto components = actor->GetAllComponents();

		for (std::size_t componentIndex = 0; componentIndex < components.size(); ++componentIndex)
		{
			Component* component = components[componentIndex];

			if (!component || component->IsDestroyed())
			{
				continue;
			}

			const std::string componentPath =
				ordinary ? actorPath + "/components/" + std::to_string(componentIndex) + "/data" : actorPath;
			json properties;

			if (!SerializeReflectedComponent(*component, properties, &scene))
			{
				return Fail(error, SceneLoadErrorCode::ReferenceResolutionFailed, componentPath,
					"Component persistence validation failed.");
			}

			if (!component->ResolveReferences(scene))
			{
				return Fail(error, SceneLoadErrorCode::ReferenceResolutionFailed, componentPath,
					"Component reference resolution failed on Actor '" + actor->GetName() + "'.");
			}
		}
	}

	return true;
}

bool SceneLoader::ValidateSceneSettings(
	const json& sceneJson,
	bool strictV4,
	SceneLoadError& error)
{
	const auto lightEntry = sceneJson.find("directional_light");

	if (lightEntry == sceneJson.end())
	{
		if (strictV4)
		{
			return Fail(error, SceneLoadErrorCode::InvalidSceneSettings, "/directional_light", "Required field is missing.");
		}

		return true;
	}

	if (!lightEntry->is_object())
	{
		return Fail(error, SceneLoadErrorCode::InvalidSceneSettings,
			"/directional_light", "Directional light must be an object.");
	}

	if (strictV4)
	{
		if (!ValidateFields(*lightEntry,
				{"direction", "color", "intensity"}, {}, "/directional_light", error))
		{
			return false;
		}
	}

	else
	{
		for (std::string_view field : {"direction", "color", "intensity"})
		{
			if (!lightEntry->contains(field))
			{
				return Fail(error, SceneLoadErrorCode::InvalidSceneSettings,
					AppendPath("/directional_light", std::string(field)), "Required field is missing.");
			}
		}
	}

	if (!ValidateVector3((*lightEntry)["direction"], "/directional_light/direction", error) ||
		!ValidateVector3((*lightEntry)["color"], "/directional_light/color", error))
	{
		return false;
	}

	const json& intensity = (*lightEntry)["intensity"];

	if (!intensity.is_number())
	{
		return Fail(error, SceneLoadErrorCode::InvalidSceneSettings,
			"/directional_light/intensity", "Directional light intensity must be a finite number.");
	}

	const double sourceIntensity = intensity.get<double>();

	if (!FitsFiniteFloat(sourceIntensity))
	{
		return Fail(error, SceneLoadErrorCode::InvalidSceneSettings,
			"/directional_light/intensity", "Directional light intensity is outside the finite float range.");
	}

	return true;
}

bool SceneLoader::ApplySceneSettings(
	const json& sceneJson,
	SceneBase& scene,
	SceneLoadError& error)
{
	const auto entry = sceneJson.find("directional_light");

	if (entry == sceneJson.end())
	{
		return true;
	}

	try
	{
		DirectionalLight light;
		light.direction = { (*entry)["direction"][0].get<float>(),
			(*entry)["direction"][1].get<float>(), (*entry)["direction"][2].get<float>() };
		light.color = { (*entry)["color"][0].get<float>(),
			(*entry)["color"][1].get<float>(), (*entry)["color"][2].get<float>() };
		light.intensity = (*entry)["intensity"].get<float>();
		scene.SetDirectionalLight(light);
		return true;
	}
	catch (const json::exception& exception)
	{
		return Fail(error, SceneLoadErrorCode::InvalidSceneSettings,
			"/directional_light", exception.what());
	}
}

bool SceneLoader::ValidateInstanceRegistry(SceneBase& scene, SceneLoadError& error)
{
	for (const auto& [root, record] : scene.GetImprintInstances().GetInstances())
	{
		if (!scene.ValidateInstanceDestruction(root))
		{
			Actor* rootActor = scene.ResolveActor(root);
			const std::string path = rootActor && rootActor->GetGuid().IsValid()
				? "/actorImprintInstances" : "/actorImprintInstances";
			(void)record;
			return Fail(error, SceneLoadErrorCode::InvalidInstanceRegistry, path,
				"ActorImprint Instance registry is inconsistent with the candidate Scene.");
		}
	}

	return true;
}

void SceneLoader::ConfigureMainCamera(SceneBase& scene, const std::vector<Actor*>& actors)
{
	for (Actor* actor : actors)
	{
		if (!actor || actor->IsDestroyed() || actor->GetTag() != ActorTags::MainCamera)
		{
			continue;
		}

		Camera* camera = actor->GetComponentByClass<Camera>();

		if (!camera)
		{
			continue;
		}

		scene.GetCameraSystem()->SetMainCamera(camera);
		return;
	}

	DBG("SceneLoader: Warning - No main camera found.");
}

void SceneLoader::ConfigureInitialSky(SceneBase& scene, const std::vector<Actor*>& actors)
{
	SkyRenderer* initialSky = nullptr;
	bool duplicate = false;

	for (Actor* actor : actors)
	{
		if (!actor || actor->IsDestroyed() || actor->GetTag() != ActorTags::InitialSky)
		{
			continue;
		}

		SkyRenderer* sky = actor->GetComponentByClass<SkyRenderer>();

		if (!sky)
		{
			continue;
		}

		if (!initialSky)
		{
			initialSky = sky;
		}

		else
		{
			duplicate = true;
		}
	}

	if (initialSky)
	{
		scene.GetRenderSystem()->SetActiveSkyRenderer(initialSky);
	}

	if (duplicate)
	{
		DBG("SceneLoader: Warning - Multiple valid InitialSky Actors found; using the first.");
	}
}
