#pragma once
#include "LocalObjectId.h"
#include "Engine/Core/Reflection/ActorReferenceCodec.h"
#include <unordered_map>

// Operation-local translation. Runtime ActorReference continues to store a Guid.
// Construct once per asset/instance operation, not once per property.
enum class ActorImprintReferenceMode
{
	DefinitionLocalOnly,
	Instance,
};

class ActorImprintReferenceCodec final : public ActorReferenceCodec
{
public:
	using ActorGuids = std::unordered_map<LocalObjectId, Guid>;
	explicit ActorImprintReferenceCodec(ActorGuids actorGuids,
		ActorImprintReferenceMode mode = ActorImprintReferenceMode::DefinitionLocalOnly);

	ActorReferenceCodecResult Serialize(const ActorReference& reference,
		const ActorReferenceSaveContext& context, nlohmann::json& outJson) const override;
	ActorReferenceCodecResult Deserialize(const nlohmann::json& json,
		ActorReference& outReference) const override;
	ActorReferenceCodecResult Resolve(ActorReference& reference,
		const ActorReferenceRestoreContext& context) const override;

private:
	ActorGuids m_actorGuids;
	std::unordered_map<Guid, LocalObjectId> m_localIds;
	ActorImprintReferenceMode m_mode = ActorImprintReferenceMode::DefinitionLocalOnly;
	bool m_valid = true;
};
