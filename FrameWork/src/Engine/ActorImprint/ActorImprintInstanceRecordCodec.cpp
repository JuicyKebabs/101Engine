#include "ActorImprintInstanceRecordCodec.h"
#include "ActorImprintPropertyOverrides.h"
#include <algorithm>
#include <initializer_list>
#include <unordered_set>

namespace
{
	using json = nlohmann::json;
	using ErrorCode = ActorImprintInstanceRecordErrorCode;

	bool Fail(ActorImprintInstanceRecordError* error, ErrorCode code,
		std::string path, std::string message, LocalObjectId target = InvalidLocalObjectId)
	{
		if (error) *error = { code, target, std::move(path), std::move(message) };
		return false;
	}

	bool Contains(std::initializer_list<const char*> fields, const std::string& field)
	{
		return std::find(fields.begin(), fields.end(), field) != fields.end();
	}

	std::string AppendPath(const std::string& base, const std::string& member)
	{
		return (json::json_pointer(base) / member).to_string();
	}

	bool ValidateFields(const json& object,
		std::initializer_list<const char*> required,
		std::initializer_list<const char*> optional,
		const std::string& path,
		ActorImprintInstanceRecordError* error,
		LocalObjectId target = InvalidLocalObjectId)
	{
		if (!object.is_object())
			return Fail(error, ErrorCode::InvalidSchema, path, "Expected an object.", target);
		for (auto member = object.begin(); member != object.end(); ++member)
		{
			if (!Contains(required, member.key()) && !Contains(optional, member.key()))
				return Fail(error, ErrorCode::InvalidSchema, AppendPath(path, member.key()),
					"Unknown field: " + member.key(), target);
		}
		for (const char* field : required)
		{
			if (!object.contains(field))
				return Fail(error, ErrorCode::InvalidSchema, AppendPath(path, field),
					"Required field is missing.", target);
		}
		return true;
	}

	bool ReadLocalObjectId(const json& source, LocalObjectId& outId)
	{
		if (!source.is_number_integer()) return false;
		LocalObjectId id = InvalidLocalObjectId;
		if (source.is_number_unsigned())
		{
			id = source.get<LocalObjectId>();
		}
		else
		{
			const auto signedId = source.get<std::int64_t>();
			if (signedId <= 0) return false;
			id = static_cast<LocalObjectId>(signedId);
		}
		if (id == InvalidLocalObjectId) return false;
		outId = id;
		return true;
	}

	bool ReadGuid(const json& source, Guid& outGuid)
	{
		if (!source.is_string()) return false;
		const std::string text = source.get<std::string>();
		return text.find('\0') == std::string::npos && Guid::TryParse(text, outGuid) && outGuid.IsValid();
	}

	bool NormalizeProperties(ActorImprintPropertyOverrideTarget& target,
		const std::string& path, ActorImprintInstanceRecordError* error)
	{
		ActorImprintPropertyOverrideError structuralError;
		if (!ActorImprintPropertyOverrides::ValidateStructure(target, &structuralError))
			return Fail(error, ErrorCode::InvalidPropertyOverride,
				structuralError.path.empty() ? path : AppendPath(path, structuralError.path),
				structuralError.message, target.targetLocalObjectId);
		std::sort(target.properties.begin(), target.properties.end(), [](const auto& left, const auto& right)
		{
			return left.path.ToString() < right.path.ToString();
		});
		for (std::size_t i = 0; i < target.properties.size(); ++i)
		{
			const auto& property = target.properties[i];
			const auto reparsed = PropertyPath::FromString(property.path.ToString());
			if (!reparsed || *reparsed != property.path)
				return Fail(error, ErrorCode::InvalidPropertyOverride, path,
					"Override path is not a normalized JSON Pointer.", target.targetLocalObjectId);
		}
		return true;
	}

	bool NormalizeRecord(ActorImprintSerializedInstanceRecord& record,
		ActorImprintInstanceRecordError* error)
	{
		if (!record.assetGuid.IsValid())
			return Fail(error, ErrorCode::InvalidAssetGuid, "/assetGuid", "Expected a nonzero ActorImprint Asset GUID.");
		if (!record.sourceDefinitionRevision.IsValid())
			return Fail(error, ErrorCode::InvalidDefinitionRevision, "/sourceDefinitionRevision",
				"Expected a nonzero DefinitionRevision.");
		if (!record.rootActorGuid.IsValid())
			return Fail(error, ErrorCode::InvalidRootActorGuid, "/rootActorGuid", "Expected a nonzero root Actor GUID.");
		if (record.externalParentActorGuid && !record.externalParentActorGuid->IsValid())
			return Fail(error, ErrorCode::InvalidExternalParentActorGuid, "/externalParentActorGuid",
				"External parent GUID must be nonzero when present.");
		if (record.actorGuids.empty())
			return Fail(error, ErrorCode::InvalidActorGuidMapping, "/actorGuids",
				"Actor GUID mapping must contain the Instance root.");

		std::unordered_set<LocalObjectId> localIds;
		std::unordered_set<Guid> guids;
		bool containsRootGuid = false;
		for (std::size_t i = 0; i < record.actorGuids.size(); ++i)
		{
			const auto& entry = record.actorGuids[i];
			const std::string path = "/actorGuids/" + std::to_string(i);
			if (entry.localObjectId == InvalidLocalObjectId)
				return Fail(error, ErrorCode::InvalidActorGuidMapping, path + "/localObjectId",
					"Actor LocalObjectID must be nonzero.");
			if (!entry.actorGuid.IsValid())
				return Fail(error, ErrorCode::InvalidActorGuidMapping, path + "/actorGuid",
					"Actor GUID must be nonzero.");
			if (!localIds.insert(entry.localObjectId).second)
				return Fail(error, ErrorCode::InvalidActorGuidMapping, path + "/localObjectId",
					"Actor LocalObjectID is duplicated.");
			if (!guids.insert(entry.actorGuid).second)
				return Fail(error, ErrorCode::InvalidActorGuidMapping, path + "/actorGuid",
					"Actor GUID is duplicated.");
			containsRootGuid = containsRootGuid || entry.actorGuid == record.rootActorGuid;
		}
		if (!containsRootGuid)
			return Fail(error, ErrorCode::InvalidRootActorGuid, "/rootActorGuid",
				"Root Actor GUID is absent from the Actor GUID mapping.");
		std::sort(record.actorGuids.begin(), record.actorGuids.end(), [](const auto& left, const auto& right)
		{
			return left.localObjectId < right.localObjectId;
		});

		std::unordered_set<LocalObjectId> targets;
		for (std::size_t i = 0; i < record.propertyOverrides.size(); ++i)
		{
			auto& target = record.propertyOverrides[i];
			const std::string path = "/propertyOverrides/" + std::to_string(i) + "/properties";
			if (target.targetLocalObjectId == InvalidLocalObjectId)
				return Fail(error, ErrorCode::InvalidPropertyOverride,
					"/propertyOverrides/" + std::to_string(i) + "/targetLocalObjectId",
					"Override target LocalObjectID must be nonzero.");
			if (!targets.insert(target.targetLocalObjectId).second)
				return Fail(error, ErrorCode::InvalidPropertyOverride,
					"/propertyOverrides/" + std::to_string(i) + "/targetLocalObjectId",
					"Override target LocalObjectID is duplicated.", target.targetLocalObjectId);
			if (!NormalizeProperties(target, path, error)) return false;
		}
		record.propertyOverrides.erase(
			std::remove_if(record.propertyOverrides.begin(), record.propertyOverrides.end(),
				[](const auto& target) { return target.properties.empty(); }),
			record.propertyOverrides.end());
		std::sort(record.propertyOverrides.begin(), record.propertyOverrides.end(), [](const auto& left, const auto& right)
		{
			return left.targetLocalObjectId < right.targetLocalObjectId;
		});
		return true;
	}
}

bool ActorImprintInstanceRecordReader::Read(const nlohmann::json& source,
	ActorImprintSerializedInstanceRecord& outRecord, ActorImprintInstanceRecordError* outError)
{
	if (outError) *outError = {};
	try
	{
		if (!ValidateFields(source,
			{ "assetGuid", "sourceDefinitionRevision", "rootActorGuid", "externalParentActorGuid", "actorGuids" },
			{ "propertyOverrides" }, "", outError)) return false;

		ActorImprintSerializedInstanceRecord record;
		if (!ReadGuid(source["assetGuid"], record.assetGuid))
			return Fail(outError, ErrorCode::InvalidAssetGuid, "/assetGuid", "Expected a nonzero ActorImprint Asset GUID string.");
		if (!source["sourceDefinitionRevision"].is_string() ||
			!DefinitionRevision::TryParse(source["sourceDefinitionRevision"].get<std::string>(), record.sourceDefinitionRevision))
			return Fail(outError, ErrorCode::InvalidDefinitionRevision, "/sourceDefinitionRevision",
				"Expected a nonzero DefinitionRevision string.");
		if (!ReadGuid(source["rootActorGuid"], record.rootActorGuid))
			return Fail(outError, ErrorCode::InvalidRootActorGuid, "/rootActorGuid", "Expected a nonzero root Actor GUID string.");

		const json& parent = source["externalParentActorGuid"];
		if (!parent.is_null())
		{
			Guid parentGuid;
			if (!ReadGuid(parent, parentGuid))
				return Fail(outError, ErrorCode::InvalidExternalParentActorGuid, "/externalParentActorGuid",
					"Expected null or a nonzero external parent Actor GUID string.");
			record.externalParentActorGuid = parentGuid;
		}

		const json& actorGuids = source["actorGuids"];
		if (!actorGuids.is_array())
			return Fail(outError, ErrorCode::InvalidActorGuidMapping, "/actorGuids", "Expected an Actor GUID mapping array.");
		for (std::size_t i = 0; i < actorGuids.size(); ++i)
		{
			const std::string path = "/actorGuids/" + std::to_string(i);
			const json& sourceEntry = actorGuids[i];
			if (!ValidateFields(sourceEntry, { "localObjectId", "actorGuid" }, {}, path, outError)) return false;
			ActorImprintActorGuidEntry entry;
			if (!ReadLocalObjectId(sourceEntry["localObjectId"], entry.localObjectId))
				return Fail(outError, ErrorCode::InvalidActorGuidMapping, path + "/localObjectId",
					"Expected a nonzero uint64 Actor LocalObjectID.");
			if (!ReadGuid(sourceEntry["actorGuid"], entry.actorGuid))
				return Fail(outError, ErrorCode::InvalidActorGuidMapping, path + "/actorGuid",
					"Expected a nonzero Actor GUID string.");
			record.actorGuids.push_back(entry);
		}

		if (const auto overrides = source.find("propertyOverrides"); overrides != source.end())
		{
			if (!overrides->is_array())
				return Fail(outError, ErrorCode::InvalidPropertyOverride, "/propertyOverrides",
					"Expected a Property Override array.");
			for (std::size_t i = 0; i < overrides->size(); ++i)
			{
				const std::string targetPath = "/propertyOverrides/" + std::to_string(i);
				const json& sourceTarget = (*overrides)[i];
				if (!ValidateFields(sourceTarget, { "targetLocalObjectId", "properties" }, {}, targetPath, outError)) return false;
				ActorImprintPropertyOverrideTarget target;
				if (!ReadLocalObjectId(sourceTarget["targetLocalObjectId"], target.targetLocalObjectId))
					return Fail(outError, ErrorCode::InvalidPropertyOverride, targetPath + "/targetLocalObjectId",
						"Expected a nonzero uint64 target LocalObjectID.");
				const json& properties = sourceTarget["properties"];
				if (!properties.is_object())
					return Fail(outError, ErrorCode::InvalidPropertyOverride, targetPath + "/properties",
						"Expected a Property Override object.", target.targetLocalObjectId);
				for (auto property = properties.begin(); property != properties.end(); ++property)
				{
					const auto path = PropertyPath::FromString(property.key());
					if (!path)
						return Fail(outError, ErrorCode::InvalidPropertyOverride,
							AppendPath(targetPath + "/properties", property.key()),
							"Override key must be a valid nonempty JSON Pointer.", target.targetLocalObjectId);
					target.properties.emplace_back(*path, property.value());
				}
				record.propertyOverrides.push_back(std::move(target));
			}
		}

		if (!NormalizeRecord(record, outError)) return false;
		outRecord = std::move(record);
		return true;
	}
	catch (const std::exception& exception)
	{
		return Fail(outError, ErrorCode::InvalidSchema, "", exception.what());
	}
}

bool ActorImprintInstanceRecordWriter::Write(const ActorImprintSerializedInstanceRecord& source,
	nlohmann::json& outJson, ActorImprintInstanceRecordError* outError)
{
	if (outError) *outError = {};
	try
	{
		ActorImprintSerializedInstanceRecord record = source;
		if (!NormalizeRecord(record, outError)) return false;

		json actorGuids = json::array();
		for (const auto& entry : record.actorGuids)
			actorGuids.push_back({ { "localObjectId", entry.localObjectId }, { "actorGuid", entry.actorGuid.ToString() } });

		json result = {
			{ "assetGuid", record.assetGuid.ToString() },
			{ "sourceDefinitionRevision", record.sourceDefinitionRevision.ToString() },
			{ "rootActorGuid", record.rootActorGuid.ToString() },
			{ "externalParentActorGuid", record.externalParentActorGuid
				? json(record.externalParentActorGuid->ToString()) : json(nullptr) },
			{ "actorGuids", std::move(actorGuids) },
		};
		if (!record.propertyOverrides.empty())
		{
			json overrides = json::array();
			for (const auto& target : record.propertyOverrides)
			{
				json properties = json::object();
				for (const auto& property : target.properties)
					properties[property.path.ToString()] = property.value;
				overrides.push_back({ { "targetLocalObjectId", target.targetLocalObjectId },
					{ "properties", std::move(properties) } });
			}
			result["propertyOverrides"] = std::move(overrides);
		}

		outJson = std::move(result);
		return true;
	}
	catch (const std::exception& exception)
	{
		return Fail(outError, ErrorCode::InvalidSchema, "", exception.what());
	}
}
