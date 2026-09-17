#pragma once

#include "LocalObjectId.h"
#include "Engine/Core/GUID/Guid.h"

#include <string>
#include <unordered_map>

class Actor;
class ActorImprint;
class Component;
class SceneBase;

struct ActorImprintDefinitionExpansion
{
	Actor* root = nullptr;
	std::unordered_map<LocalObjectId, Guid> actorGuids;
	std::unordered_map<LocalObjectId, Component*> components;
};

// Expands one validated immutable definition into ordinary Scene objects. This
// deliberately does not create ActorImprint Instance provenance or register an
// Instance. The caller owns the destination Scene and its authoring policy.
class ActorImprintDefinitionExpander
{
public:
	static bool Expand(
		const ActorImprint& definition,
		SceneBase& destination,
		ActorImprintDefinitionExpansion& outExpansion);
};
