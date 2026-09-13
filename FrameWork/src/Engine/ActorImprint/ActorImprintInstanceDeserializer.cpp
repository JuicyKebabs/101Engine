#include "ActorImprintInstanceDeserializer.h"
#include "ActorImprintInstanceRecordCodec.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

namespace
{
	using ErrorCode = ActorImprintInstanceDeserializationErrorCode;

	bool Fail(ActorImprintInstanceDeserializationError* error, ErrorCode code,
		LocalObjectId id, std::string path, std::string message)
	{
		if (error) *error = { code, id, std::move(path), std::move(message) };
		return false;
	}
}

bool ActorImprintInstanceDeserializer::Prepare(const ActorImprintSerializedInstanceRecord& record,
	SceneBase& scene, ActorImprintSystem& system, ActorImprintPreparedRestore& outRestore,
	ActorImprintInstanceDeserializationError* outError)
{
	if (outError) *outError = {};
	try
	{
		// Validate programmatically-created records through the same strict persistence boundary.
		nlohmann::json validatedJson;
		ActorImprintInstanceRecordError recordError;
		if (!ActorImprintInstanceRecordWriter::Write(record, validatedJson, &recordError))
			return Fail(outError, ErrorCode::InvalidRecord, recordError.targetLocalObjectId,
				recordError.path, recordError.message);

		ActorImprintLoadError loadError;
		const ActorImprintHandle imprint = system.Load(record.assetGuid, &loadError);
		if (imprint.IsNull())
			return Fail(outError, ErrorCode::InvalidAsset, 0, "/assetGuid", loadError.message);
		ActorHandle externalParent;
		if (record.externalParentActorGuid)
		{
			Actor* parent = scene.ResolveActor(*record.externalParentActorGuid);
			if (!parent || parent->IsDestroyed() || scene.GetImprintInstances().FindMember(parent->GetHandle()))
				return Fail(outError, ErrorCode::InvalidExternalParent, 0, "/externalParentActorGuid",
					"External parent must resolve to a live ordinary Actor in the destination Scene.");
			externalParent = parent->GetHandle();
		}

		ActorImprintPreparedRestore candidate;
		candidate.imprint = imprint;
		candidate.input.sourceDefinitionRevision = record.sourceDefinitionRevision;
		candidate.input.rootActorGuid = record.rootActorGuid;
		candidate.input.externalParent = externalParent;
		candidate.input.propertyOverrides = record.propertyOverrides;
		candidate.input.actorGuids.reserve(record.actorGuids.size());
		for (const auto& actor : record.actorGuids)
			candidate.input.actorGuids.emplace(actor.localObjectId, actor.actorGuid);
		outRestore = std::move(candidate);
		return true;
	}
	catch (const std::exception& error)
	{
		return Fail(outError, ErrorCode::InvalidRecord, 0, {}, error.what());
	}
}

bool ActorImprintInstanceDeserializer::Deserialize(const nlohmann::json& json, SceneBase& scene,
	ActorImprintSystem& system, ActorImprintPreparedRestore& outRestore,
	ActorImprintInstanceDeserializationError* outError)
{
	ActorImprintSerializedInstanceRecord record;
	ActorImprintInstanceRecordError recordError;
	if (!ActorImprintInstanceRecordReader::Read(json, record, &recordError))
		return Fail(outError, ErrorCode::InvalidRecord, recordError.targetLocalObjectId,
			recordError.path, recordError.message);
	return Prepare(record, scene, system, outRestore, outError);
}

Actor* ActorImprintInstanceDeserializer::Restore(const nlohmann::json& json, SceneBase& scene,
	ActorImprintSystem& system, ActorImprintInstanceDeserializationError* outError)
{
	ActorImprintPreparedRestore restore;
	if (!Deserialize(json, scene, system, restore, outError)) return nullptr;
	ActorImprintMaterializationError materializationError;
	Actor* actor = system.RestoreInstance(scene, restore.imprint, restore.input, &materializationError);
	if (actor) return actor;
	Fail(outError, ErrorCode::MaterializationFailed, materializationError.objectId,
		materializationError.path, materializationError.message);
	return nullptr;
}
