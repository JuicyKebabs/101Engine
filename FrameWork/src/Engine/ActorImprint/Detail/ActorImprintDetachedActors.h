#pragma once
#include "Engine/Actor/Actor.h"
#include "Engine/ActorImprint/ActorImprintInstanceRecord.h"
#include "Engine/ActorImprint/ActorImprintPropertyOverrides.h"
#include "Engine/ActorImprint/ActorImprintReferenceCodec.h"
#include <optional>

class ActorImprint;
class SceneBase;

namespace ActorImprintDetail
{
	using DetachedActors = std::vector<std::unique_ptr<Actor>>;

	// The result has exactly the immutable definition's Actor order. It owns only
	// detached objects: no Scene handles, hierarchy links, reference resolution or
	// lifecycle callbacks are established here. This is not an Instance commit.
	// A null restoredGuids map means new identities; otherwise every Actor must
	// have exactly one supplied identity. Failure returns no partial candidate.
	std::optional<DetachedActors> CreateDetachedActors(
		const ActorImprint& definition,
		const SceneBase& destination,
		const ActorImprintReferenceCodec::ActorGuids* restoredGuids,
		const std::vector<ActorImprintPropertyOverrideTarget>* overrides,
		ActorImprintOverrideRevisionRelation revisionRelation);
	inline std::optional<DetachedActors> CreateDetachedActors(
		const ActorImprint& definition,
		const SceneBase& destination,
		const ActorImprintReferenceCodec::ActorGuids* restoredGuids)
	{
		return CreateDetachedActors(definition, destination, restoredGuids, nullptr,
			ActorImprintOverrideRevisionRelation::Same);
	}
}
