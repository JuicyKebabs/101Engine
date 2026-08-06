#include "ActorSubtreeSnapshot.h"
#include "Engine/Scene/ActorSerializer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Core/Debug/Debug.h"
#include <unordered_set>

using json = nlohmann::json;

bool ActorSubtreeSnapshot::Capture(Actor* rootActor, SceneBase* scene)
{
	Clear();	// Clear any previous snapshot data

	// Validate the input parameters
	if (!rootActor)
	{
		DBG("ActorSubtreeSnapshot::Capture: Root Actor is null.");
		return false;
	}

	if (!scene)
	{
		DBG("ActorSubtreeSnapshot::Capture: Scene is null.");
		return false;
	}

	if (rootActor->IsDestroyed())
	{
		DBG("ActorSubtreeSnapshot::Capture: Root Actor is already destroyed.");
		return false;
	}

	if (rootActor->GetOwner() != scene)
	{
		DBG("ActorSubtreeSnapshot::Capture: Root Actor does not belong to the given Scene.");
		return false;
	}

	if (!rootActor->GetGuid().IsValid())
	{
		DBG("ActorSubtreeSnapshot::Capture: Root Actor has an invalid Guid.");
		return false;
	}

	std::vector<json> capturedRecords;			// Temporary result buffer for serialized Actor records
	std::vector<Actor*> pendingActors;			// Stack to hold actors pending serialization
	std::unordered_set<Guid> visitedActorIds;	// Set to track visited actors and prevent cycles

	pendingActors.push_back(rootActor);

	// Process actors by DFS (Depth-First Search)
	while (!pendingActors.empty())
	{
		// Pop the last actor from the pending stack for processing
		Actor* actor = pendingActors.back();
		pendingActors.pop_back();

		// Validate actor before capturing its data
		if (!actor)
		{
			DBG("ActorSubtreeSnapshot::Capture: Encountered a null Actor.");
			return false;
		}

		if (actor->IsDestroyed())
		{
			DBG(
				"ActorSubtreeSnapshot::Capture: Actor '%s' is already destroyed.",
				actor->GetName().c_str());
			return false;
		}

		if (actor->GetOwner() != scene)
		{
			DBG(
				"ActorSubtreeSnapshot::Capture: Actor '%s' belongs to another Scene.",
				actor->GetName().c_str());
			return false;
		}

		// Get Guid and validate it
		const Guid& actorId = actor->GetGuid();
		if (!actorId.IsValid())
		{
			DBG("ActorSubtreeSnapshot::Capture: Actor '%s' has an invalid Guid.", actor->GetName().c_str());
			return false;
		}

		// Check for duplicate or cyclic relationships using the visitedActorIds set
		if (visitedActorIds.insert(actorId).second == false)
		{
			DBG("ActorSubtreeSnapshot::Capture: Duplicate or cyclic Actor relationship detected at '%s'.", actor->GetName().c_str());
			return false;
		}

		// Serialize actor data into JSON and store it in the captured records
		json actorRecord;
		if (!ActorSerializer::SerializeActorRecord(actor, scene, actorRecord))
		{
			DBG("ActorSubtreeSnapshot::Capture: Failed to serialize Actor '%s'.", actor->GetName().c_str());
			return false;
		}

		capturedRecords.push_back(std::move(actorRecord));

		const std::vector<ActorHandle> childrenHandles = actor->GetChildrenHandles();

		for (auto it = childrenHandles.rbegin(); it != childrenHandles.rend(); ++it)
		{
			Actor* child = scene->ResolveActor(*it);

			if (!child)
			{
				DBG("ActorSubtreeSnapshot::Capture: A child of Actor '%s' cannot be resolved.", actor->GetName().c_str());
				return false;
			}

			// Check consistency of parent-child relationship
			if (child->GetParentHandle() != actor->GetHandle())
			{
				DBG("ActorSubtreeSnapshot::Capture: Parent-child relationship is inconsistent for Actor '%s'.", child->GetName().c_str());
				return false;
			}

			pendingActors.push_back(child);
		}
	} 

	m_rootActorId = rootActor->GetGuid();

	// Store the captured actor records in the snapshot
	// after all actors have been processed correctly
	m_actorRecords = std::move(capturedRecords);

	return true;
}

void ActorSubtreeSnapshot::Clear()
{
	m_rootActorId = {};
	m_actorRecords.clear();
}
