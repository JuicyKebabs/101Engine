#include "ActorImprintInstanceDeserializer.h"
#include "Engine/Core/Debug/Debug.h"
#include "ActorImprintInstanceRecordCodec.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"

bool ActorImprintInstanceDeserializer::Prepare(
	const ActorImprintSerializedInstanceRecord& record,
	SceneBase& scene,
	ActorImprintSystem& system,
	ActorImprintPreparedRestore& outRestore)
{
	try
	{
		// Validate programmatically-created records through the same strict persistence boundary.
		nlohmann::json validatedJson;

		if (!ActorImprintInstanceRecordWriter::Write(record, validatedJson))
		{
			return false;
		}

		const ActorImprintHandle imprint = system.Load(record.assetGuid);

		if (imprint.IsNull())
		{
			return false;
		}

		ActorHandle externalParent;

		if (record.externalParentActorGuid)
		{
			Actor* parent = scene.ResolveActor(*record.externalParentActorGuid);

			if (!parent || parent->IsDestroyed() || scene.GetImprintInstances().FindMember(parent->GetHandle()))
			{
				DBG("External parent must resolve to a live ordinary Actor in the destination Scene.");
				return false;
			}

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
		{
			candidate.input.actorGuids.emplace(actor.localObjectId, actor.actorGuid);
		}

		outRestore = std::move(candidate);
		return true;
	}
	catch (const std::exception& error)
	{
		DBG("%s", error.what());
		return false;
	}
}

bool ActorImprintInstanceDeserializer::Deserialize(
	const nlohmann::json& json,
	SceneBase& scene,
	ActorImprintSystem& system,
	ActorImprintPreparedRestore& outRestore)
{
	ActorImprintSerializedInstanceRecord record;

	if (!ActorImprintInstanceRecordReader::Read(json, record))
	{
		return false;
	}

	return Prepare(record, scene, system, outRestore);
}

Actor* ActorImprintInstanceDeserializer::Restore(
	const nlohmann::json& json,
	SceneBase& scene,
	ActorImprintSystem& system)
{
	ActorImprintPreparedRestore restore;

	if (!Deserialize(json, scene, system, restore))
	{
		return nullptr;
	}

	Actor* actor = system.RestoreInstance(scene, restore.imprint, restore.input);

	if (actor)
	{
		return actor;
	}

	return nullptr;
}
