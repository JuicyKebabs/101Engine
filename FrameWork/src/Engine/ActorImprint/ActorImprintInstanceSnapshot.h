#pragma once
#include "ActorImprintInstanceDeserializer.h"
#include "ActorImprintInstanceSerializer.h"
#include <optional>

class ActorImprintInstanceSnapshot
{
public:
	bool Capture(const SceneBase& scene, ActorHandle root);
	Actor* Restore(SceneBase& scene, ActorImprintSystem& system) const;
	const ActorImprintSerializedInstanceRecord* GetRecord() const
	{
		return m_record ? &*m_record : nullptr;
	}
	void Clear() { m_record.reset(); }

private:
	std::optional<ActorImprintSerializedInstanceRecord> m_record;
};
