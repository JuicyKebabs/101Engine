#include "ActorReferenceCodec.h"
#include "Engine/Actor/Actor.h"
#include "nlohmann/json.hpp"

ActorReferenceCodecResult GuidActorReferenceCodec::Serialize(
	const ActorReference& reference,
	const ActorReferenceSaveContext& context,
	nlohmann::json& outJson) const
{
	if (!reference.HasValue())
	{// If the reference is empty, serialize it as null.
		outJson = nullptr;
		return ActorReferenceCodecResult::Success;
	}

	// Check if the Guid of given reference exists in the context scene
	const ActorReferenceCodecResult result = context.Validate(reference.GetGuid());
	if (result != ActorReferenceCodecResult::Success)
	{
		return result;
	}

	outJson = reference.GetGuid().ToString();
	return ActorReferenceCodecResult::Success;
}

ActorReferenceCodecResult GuidActorReferenceCodec::Deserialize(
	const nlohmann::json& json,
	ActorReference& outReference) const
{
	if (json.is_null())
	{// Return an empty ActorReference if the JSON is null.
		outReference.Clear();
		return ActorReferenceCodecResult::Success;
	}

	if (!json.is_string()) return ActorReferenceCodecResult::InvalidJsonType;

	Guid guid;

	// Parse the string from JSON to Guid.
	if (!Guid::TryParse(json.get<std::string>(), guid))
	{
		return ActorReferenceCodecResult::InvalidGuid;
	}

	// Do not set the outREference brfore validating the guid,
	ActorReference result;
	if (!result.SetGuid(guid)) return ActorReferenceCodecResult::InvalidGuid;

	outReference = result;
	return ActorReferenceCodecResult::Success;
}

ActorReferenceCodecResult GuidActorReferenceCodec::Resolve(
	ActorReference& reference,
	const ActorReferenceRestoreContext& context) const
{
	// Not neccessary to resolve an empty ActorReference, so return success.
	if (!reference.HasValue()) return ActorReferenceCodecResult::Success;

	Actor* actor = nullptr;

	// Check if the Actor which has given Guid exists in the context scene and set the ActorReference to it.
	const ActorReferenceCodecResult result = context.FindActor(reference.GetGuid(), actor);
	if (result != ActorReferenceCodecResult::Success) return result;

	return actor && reference.Set(actor)
		? ActorReferenceCodecResult::Success
		: ActorReferenceCodecResult::ActorNotFound;
}
