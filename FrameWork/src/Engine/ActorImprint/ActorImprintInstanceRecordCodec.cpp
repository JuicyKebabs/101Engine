#include "ActorImprintInstanceRecordCodec.h"
#include "Engine/Core/Debug/Debug.h"
#include "ActorImprintPropertyOverrides.h"
#include <algorithm>
#include <initializer_list>
#include <unordered_set>

namespace
{
	using json = nlohmann::json;

	bool Contains(std::initializer_list<const char*> fields, const std::string& field)
	{
		return std::find(fields.begin(), fields.end(), field) != fields.end();
	}

	bool ValidateFields(
		const json& object,
		std::initializer_list<const char*> required,
		std::initializer_list<const char*> optional)
	{
		if (!object.is_object())
		{
			DBG("Expected an object.");
			return false;
		}

		for (auto member = object.begin(); member != object.end(); ++member)
		{
			if (!Contains(required, member.key()) && !Contains(optional, member.key()))
			{
				DBG("Unknown field: %s", member.key().c_str());
				return false;
			}
		}

		for (const char* field : required)
		{
			if (!object.contains(field))
			{
				DBG("Required field is missing.");
				return false;
			}
		}

		return true;
	}

	bool ReadLocalObjectId(const json& source, LocalObjectId& outId)
	{
		if (!source.is_number_integer())
		{
			return false;
		}

		LocalObjectId id = InvalidLocalObjectId;

		if (source.is_number_unsigned())
		{
			id = source.get<LocalObjectId>();
		}
		else
		{
			const auto signedId = source.get<std::int64_t>();

			if (signedId <= 0)
			{
				return false;
			}

			id = static_cast<LocalObjectId>(signedId);
		}

		if (id == InvalidLocalObjectId)
		{
			return false;
		}

		outId = id;
		return true;
	}

	bool ReadGuid(const json& source, Guid& outGuid)
	{
		if (!source.is_string())
		{
			return false;
		}

		const std::string text = source.get<std::string>();
		return text.find('\0') == std::string::npos && Guid::TryParse(text, outGuid) && outGuid.IsValid();
	}

	bool NormalizeProperties(ActorImprintPropertyOverrideTarget& target)
	{
		if (!ActorImprintPropertyOverrides::ValidateStructure(target))
		{
			return false;
		}

		std::sort(target.properties.begin(), target.properties.end(), [](const auto& left, const auto& right)
		{
			return left.path.ToString() < right.path.ToString();
		});

		for (std::size_t i = 0; i < target.properties.size(); ++i)
		{
			const auto& property = target.properties[i];
			const auto reparsed = PropertyPath::FromString(property.path.ToString());

			if (!reparsed || *reparsed != property.path)
			{
				DBG("Override path is not a normalized JSON Pointer.");
				return false;
			}
		}

		return true;
	}

	bool NormalizeRecord(ActorImprintSerializedInstanceRecord& record)
	{
		if (!record.assetGuid.IsValid())
		{
			DBG("Expected a nonzero ActorImprint Asset GUID.");
			return false;
		}

		if (!record.sourceDefinitionRevision.IsValid())
		{
			DBG("Expected a nonzero DefinitionRevision.");
			return false;
		}

		if (!record.rootActorGuid.IsValid())
		{
			DBG("Expected a nonzero root Actor GUID.");
			return false;
		}

		if (record.externalParentActorGuid && !record.externalParentActorGuid->IsValid())
		{
			DBG("External parent GUID must be nonzero when present.");
			return false;
		}

		if (record.actorGuids.empty())
		{
			DBG("Actor GUID mapping must contain the Instance root.");
			return false;
		}

		std::unordered_set<LocalObjectId> localIds;
		std::unordered_set<Guid> guids;
		bool containsRootGuid = false;

		for (std::size_t i = 0; i < record.actorGuids.size(); ++i)
		{
			const auto& entry = record.actorGuids[i];

			if (entry.localObjectId == InvalidLocalObjectId)
			{
				DBG("Actor LocalObjectID must be nonzero.");
				return false;
			}

			if (!entry.actorGuid.IsValid())
			{
				DBG("Actor GUID must be nonzero.");
				return false;
			}

			if (!localIds.insert(entry.localObjectId).second)
			{
				DBG("Actor LocalObjectID is duplicated.");
				return false;
			}

			if (!guids.insert(entry.actorGuid).second)
			{
				DBG("Actor GUID is duplicated.");
				return false;
			}

			containsRootGuid = containsRootGuid || entry.actorGuid == record.rootActorGuid;
		}

		if (!containsRootGuid)
		{
			DBG("Root Actor GUID is absent from the Actor GUID mapping.");
			return false;
		}

		std::sort(record.actorGuids.begin(), record.actorGuids.end(), [](const auto& left, const auto& right)
		{
			return left.localObjectId < right.localObjectId;
		});

		std::unordered_set<LocalObjectId> targets;

		for (std::size_t i = 0; i < record.propertyOverrides.size(); ++i)
		{
			auto& target = record.propertyOverrides[i];

			if (target.targetLocalObjectId == InvalidLocalObjectId)
			{
				DBG("Override target LocalObjectID must be nonzero.");
				return false;
			}

			if (!targets.insert(target.targetLocalObjectId).second)
			{
				DBG("Override target LocalObjectID is duplicated.");
				return false;
			}

			if (!NormalizeProperties(target))
			{
				return false;
			}
		}

		record.propertyOverrides.erase(std::remove_if(record.propertyOverrides.begin(), record.propertyOverrides.end(),
										   [](const auto& target)
		{
			return target.properties.empty();
		}),
			record.propertyOverrides.end());
		std::sort(record.propertyOverrides.begin(), record.propertyOverrides.end(),
			[](const auto& left, const auto& right)
		{
			return left.targetLocalObjectId < right.targetLocalObjectId;
		});
		return true;
	}
}

bool ActorImprintInstanceRecordReader::Read(
	const nlohmann::json& source,
	ActorImprintSerializedInstanceRecord& outRecord)
{
	try
	{
		if (!ValidateFields(source,
				{"assetGuid", "sourceDefinitionRevision", "rootActorGuid", "externalParentActorGuid", "actorGuids"},
				{"propertyOverrides"}))
		{
			return false;
		}

		ActorImprintSerializedInstanceRecord record;

		if (!ReadGuid(source["assetGuid"], record.assetGuid))
		{
			DBG("Expected a nonzero ActorImprint Asset GUID string.");
			return false;
		}

		if (!source["sourceDefinitionRevision"].is_string() ||
			!DefinitionRevision::TryParse(
				source["sourceDefinitionRevision"].get<std::string>(), record.sourceDefinitionRevision))
		{
			DBG("Expected a nonzero DefinitionRevision string.");
			return false;
		}

		if (!ReadGuid(source["rootActorGuid"], record.rootActorGuid))
		{
			DBG("Expected a nonzero root Actor GUID string.");
			return false;
		}

		const json& parent = source["externalParentActorGuid"];

		if (!parent.is_null())
		{
			Guid parentGuid;

			if (!ReadGuid(parent, parentGuid))
			{
				DBG("Expected null or a nonzero external parent Actor GUID string.");
				return false;
			}

			record.externalParentActorGuid = parentGuid;
		}

		const json& actorGuids = source["actorGuids"];

		if (!actorGuids.is_array())
		{
			DBG("Expected an Actor GUID mapping array.");
			return false;
		}

		for (std::size_t i = 0; i < actorGuids.size(); ++i)
		{
			const json& sourceEntry = actorGuids[i];

			if (!ValidateFields(sourceEntry, {"localObjectId", "actorGuid"}, {}))
			{
				return false;
			}

			ActorImprintActorGuidEntry entry;

			if (!ReadLocalObjectId(sourceEntry["localObjectId"], entry.localObjectId))
			{
				DBG("Expected a nonzero uint64 Actor LocalObjectID.");
				return false;
			}

			if (!ReadGuid(sourceEntry["actorGuid"], entry.actorGuid))
			{
				DBG("Expected a nonzero Actor GUID string.");
				return false;
			}

			record.actorGuids.push_back(entry);
		}

		if (const auto overrides = source.find("propertyOverrides"); overrides != source.end())
		{
			if (!overrides->is_array())
			{
				DBG("Expected a Property Override array.");
				return false;
			}

			for (std::size_t i = 0; i < overrides->size(); ++i)
			{
				const json& sourceTarget = (*overrides)[i];

				if (!ValidateFields(sourceTarget, {"targetLocalObjectId", "properties"}, {}))
				{
					return false;
				}

				ActorImprintPropertyOverrideTarget target;

				if (!ReadLocalObjectId(sourceTarget["targetLocalObjectId"], target.targetLocalObjectId))
				{
					DBG("Expected a nonzero uint64 target LocalObjectID.");
					return false;
				}

				const json& properties = sourceTarget["properties"];

				if (!properties.is_object())
				{
					DBG("Expected a Property Override object.");
					return false;
				}

				for (auto property = properties.begin(); property != properties.end(); ++property)
				{
					const auto path = PropertyPath::FromString(property.key());

					if (!path)
					{
						DBG("Override key must be a valid nonempty JSON Pointer.");
						return false;
					}

					target.properties.emplace_back(*path, property.value());
				}

				record.propertyOverrides.push_back(std::move(target));
			}
		}

		if (!NormalizeRecord(record))
		{
			return false;
		}

		outRecord = std::move(record);
		return true;
	}
	catch (const std::exception& exception)
	{
		DBG("%s", exception.what());
		return false;
	}
}

bool ActorImprintInstanceRecordWriter::Write(
	const ActorImprintSerializedInstanceRecord& source,
	nlohmann::json& outJson)
{
	try
	{
		ActorImprintSerializedInstanceRecord record = source;

		if (!NormalizeRecord(record))
		{
			return false;
		}

		json actorGuids = json::array();

		for (const auto& entry : record.actorGuids)
		{
			actorGuids.push_back({{"localObjectId", entry.localObjectId}, {"actorGuid", entry.actorGuid.ToString()}});
		}

		json result = {
			{"assetGuid", record.assetGuid.ToString()},
			{"sourceDefinitionRevision", record.sourceDefinitionRevision.ToString()},
			{"rootActorGuid", record.rootActorGuid.ToString()},
			{"externalParentActorGuid",
				record.externalParentActorGuid ? json(record.externalParentActorGuid->ToString()) : json(nullptr)},
			{"actorGuids", std::move(actorGuids)},
		};

		if (!record.propertyOverrides.empty())
		{
			json overrides = json::array();

			for (const auto& target : record.propertyOverrides)
			{
				json properties = json::object();

				for (const auto& property : target.properties)
				{
					properties[property.path.ToString()] = property.value;
				}

				overrides.push_back(
					{{"targetLocalObjectId", target.targetLocalObjectId}, {"properties", std::move(properties)}});
			}

			result["propertyOverrides"] = std::move(overrides);
		}

		outJson = std::move(result);
		return true;
	}
	catch (const std::exception& exception)
	{
		DBG("%s", exception.what());
		return false;
	}
}
