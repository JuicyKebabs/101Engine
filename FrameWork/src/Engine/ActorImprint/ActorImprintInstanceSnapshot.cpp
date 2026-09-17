#include "ActorImprintInstanceSnapshot.h"
#include "Engine/Core/Debug/Debug.h"

bool ActorImprintInstanceSnapshot::Capture(const SceneBase& scene, ActorHandle root)
{
	ActorImprintSerializedInstanceRecord candidate;

	if (!ActorImprintInstanceSerializer::Capture(scene, root, candidate))
	{
		return false;
	}

	m_record = std::move(candidate);
	return true;
}

Actor* ActorImprintInstanceSnapshot::Restore(SceneBase& scene, ActorImprintSystem& system) const
{
	if (!m_record)
	{
		DBG("Instance snapshot is empty.");
		return nullptr;
	}

	ActorImprintPreparedRestore prepared;

	if (!ActorImprintInstanceDeserializer::Prepare(*m_record, scene, system, prepared))
	{
		return nullptr;
	}

	Actor* restored = system.RestoreInstance(scene, prepared.imprint, prepared.input);
	return restored;
}
