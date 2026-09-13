#pragma once
#include "ActorImprintInstanceRecord.h"
#include <cstddef>

class TypeMetadata;

enum class ActorImprintOverrideRevisionRelation
{
	Same,
	Different,
};

enum class ActorImprintPropertyOverrideErrorCode
{
	None,
	InvalidTarget,
	InvalidDefaultSchema,
	CurrentSchemaMismatch,
	InvalidPath,
	ArrayElementPath,
	MissingProperty,
	IncompatibleRepresentation,
};

struct ActorImprintPropertyOverrideError
{
	ActorImprintPropertyOverrideErrorCode code = ActorImprintPropertyOverrideErrorCode::None;
	LocalObjectId targetLocalObjectId = InvalidLocalObjectId;
	std::string path; // Property JSON Pointer within the target object.
	std::string message;
};

struct ActorImprintPropertyOverrideMigration
{
	std::size_t applied = 0;
	std::size_t staleMissingPath = 0;
	std::size_t staleIncompatible = 0;
};

class ActorImprintPropertyOverrides
{
public:
	// Validates revision-independent record structure. This must run even when the
	// target no longer exists and will otherwise be classified as stale.
	static bool ValidateStructure(const ActorImprintPropertyOverrideTarget& overrides,
		ActorImprintPropertyOverrideError* outError = nullptr);

	// Both inputs must be normalized complete Reflection objects for metadata.
	// On failure, outTarget is unchanged.
	static bool Diff(const TypeMetadata& metadata,
		LocalObjectId targetLocalObjectId,
		const nlohmann::json& normalizedDefault,
		const nlohmann::json& normalizedCurrent,
		ActorImprintPropertyOverrideTarget& outTarget,
		ActorImprintPropertyOverrideError* outError = nullptr);

	// This classifies only missing paths and incompatible outer JSON representations
	// as stale when revisions differ. The caller must fully deserialize and validate
	// outCompleted; any such failure remains fatal regardless of revision relation.
	// All output arguments are unchanged on failure.
	static bool Merge(const TypeMetadata& metadata,
		const nlohmann::json& normalizedDefault,
		const ActorImprintPropertyOverrideTarget& overrides,
		ActorImprintOverrideRevisionRelation revisions,
		nlohmann::json& outCompleted,
		std::vector<ActorImprintPropertyOverrideEntry>* outApplied = nullptr,
		ActorImprintPropertyOverrideMigration* outMigration = nullptr,
		ActorImprintPropertyOverrideError* outError = nullptr);
};
