#include "ActorReferenceCodec.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Actor/Actor.h"
#include "nlohmann/json.hpp"

bool GuidActorReferenceCodec::Serialize(
	const ActorReference& reference,
	const ActorReferenceSaveContext& context,
	nlohmann::json& outJson) const
{
	if (!reference.HasValue())
	{// If the reference is empty, serialize it as null.
		outJson = nullptr;
		return true;
	}

	// Check if the Guid of given reference exists in the context scene
	const bool result = context.Validate(reference.GetGuid());

	if (!result)
	{
		return result;
	}

	outJson = reference.GetGuid().ToString();
	return true;
}

bool GuidActorReferenceCodec::Deserialize(
	const nlohmann::json& json,
	ActorReference& outReference) const
{
	if (json.is_null())
	{// Return an empty ActorReference if the JSON is null.
		outReference.Clear();
		return true;
	}

	if (!json.is_string())
	{
		DBG("Actor reference: InvalidJsonType.");
		return false;
	}

	Guid guid;

	// Parse the string from JSON to Guid.
	if (!Guid::TryParse(json.get<std::string>(), guid))
	{
		DBG("Actor reference: InvalidGuid.");
		return false;
	}

	// Do not set the outREference brfore validating the guid,
	ActorReference result;

	if (!result.SetGuid(guid))
	{
		DBG("Actor reference: InvalidGuid.");
		return false;
	}

	outReference = result;
	return true;
}

bool GuidActorReferenceCodec::Resolve(
	ActorReference& reference,
	const ActorReferenceRestoreContext& context) const
{
	// Not neccessary to resolve an empty ActorReference, so return success.
	if (!reference.HasValue())
	{
		return true;
	}

	Actor* actor = nullptr;

	// Check if the Actor which has given Guid exists in the context scene and set the ActorReference to it.
	const bool result = context.FindActor(reference.GetGuid(), actor);

	if (!result)
	{
		return result;
	}

	return actor && reference.Set(actor);
}
