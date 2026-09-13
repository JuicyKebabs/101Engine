#pragma once
#include "DefinitionRevision.h"
#include "LocalObjectId.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Core/Reflection/PropertyPath.h"
#include "nlohmann/json.hpp"
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Persistent ActorImprint Instance data. This is deliberately separate from the
// Scene-owned runtime registry record: live Actor/Component values remain the
// source of truth for property overrides.
struct ActorImprintActorGuidEntry
{
	LocalObjectId localObjectId = InvalidLocalObjectId;
	Guid actorGuid;
};

struct ActorImprintPropertyOverrideEntry
{
	ActorImprintPropertyOverrideEntry(PropertyPath propertyPath, nlohmann::json propertyValue)
		: path(std::move(propertyPath)), value(std::move(propertyValue))
	{}

	PropertyPath path;
	nlohmann::json value;
};

struct ActorImprintPropertyOverrideTarget
{
	LocalObjectId targetLocalObjectId = InvalidLocalObjectId;
	std::vector<ActorImprintPropertyOverrideEntry> properties;
};

struct ActorImprintSerializedInstanceRecord
{
	Guid assetGuid;
	DefinitionRevision sourceDefinitionRevision;
	Guid rootActorGuid;
	std::optional<Guid> externalParentActorGuid;
	std::vector<ActorImprintActorGuidEntry> actorGuids;
	std::vector<ActorImprintPropertyOverrideTarget> propertyOverrides;
};

enum class ActorImprintInstanceRecordErrorCode
{
	None,
	InvalidSchema,
	InvalidAssetGuid,
	InvalidDefinitionRevision,
	InvalidRootActorGuid,
	InvalidExternalParentActorGuid,
	InvalidActorGuidMapping,
	InvalidPropertyOverride,
};

struct ActorImprintInstanceRecordError
{
	ActorImprintInstanceRecordErrorCode code = ActorImprintInstanceRecordErrorCode::None;
	LocalObjectId targetLocalObjectId = InvalidLocalObjectId;
	std::string path; // JSON Pointer into the Instance record.
	std::string message;
};
