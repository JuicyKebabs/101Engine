#pragma once
#include <vector>
#include "Engine/Core/GUID/Guid.h"
#include "nlohmann/json.hpp"

class Actor;
class SceneBase;

//------------------------------------------------------------------------------------
// ActorSubtreeSnapshot class
// This class stores serialized data for one Actor and its hierarchy of children.
// Actors are stored in parent-first order and references are identified by their GUIDs.
//------------------------------------------------------------------------------------

class ActorSubtreeSnapshot
{
public:
	bool Capture(Actor* rootActor, SceneBase* scene);

	void Clear();

	bool IsValid() const { return m_rootActorId.IsValid() && !m_actorRecords.empty();; }

	const Guid& GetRootActorId() const { return m_rootActorId; }

	const std::vector<nlohmann::json>& GetActorRecords() const { return m_actorRecords; }


private:
	// The GUID of the root actor of the subtree(Never store ownership)
	Guid m_rootActorId;

	// Serialized actor records
	std::vector<nlohmann::json> m_actorRecords;
};
