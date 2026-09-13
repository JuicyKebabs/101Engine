#include "ActorImprintInstanceSnapshot.h"

bool ActorImprintInstanceSnapshot::Capture(const SceneBase& scene, ActorHandle root,
	ActorImprintInstanceSerializationError* outError)
{
	ActorImprintSerializedInstanceRecord candidate;
	if (!ActorImprintInstanceSerializer::Capture(scene, root, candidate, outError)) return false;
	m_record = std::move(candidate);
	return true;
}

Actor* ActorImprintInstanceSnapshot::Restore(SceneBase& scene, ActorImprintSystem& system,
	ActorImprintInstanceDeserializationError* outError) const
{
	if (!m_record)
	{
		if (outError) *outError = { ActorImprintInstanceDeserializationErrorCode::InvalidRecord,
			0, {}, "Instance snapshot is empty." };
		return nullptr;
	}
	ActorImprintPreparedRestore prepared;
	if (!ActorImprintInstanceDeserializer::Prepare(*m_record, scene, system, prepared, outError)) return nullptr;
	ActorImprintMaterializationError materializationError;
	Actor* restored = system.RestoreInstance(scene, prepared.imprint, prepared.input, &materializationError);
	if (!restored && outError)
		*outError = { ActorImprintInstanceDeserializationErrorCode::MaterializationFailed,
			materializationError.objectId, materializationError.path, materializationError.message };
	return restored;
}
