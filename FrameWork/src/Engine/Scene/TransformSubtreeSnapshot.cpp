#include "TransformSubtreeSnapshot.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Debug/Debug.h"
#include <unordered_set>

bool TransformSubtreeSnapshot::Capture(Actor* rootActor, SceneBase* scene)
{
	Clear(); // Clear any existing snapshot data

	// Validate input parameters
	if (!rootActor || !scene)
	{
		DBG("TransformSubtreeSnapshot::Capture: Actor or Scene is null.");
		return false;
	}

	if (rootActor->GetOwner() != scene || rootActor->IsDestroyed())
	{
		DBG("TransformSubtreeSnapshot::Capture: Root Actor is not available in the given Scene.");
		return false;
	}

	std::vector<Record> capturedRecords;
	std::vector<Actor*> pendingActors;
	std::unordered_set<Guid> visitedActorIds;

	pendingActors.push_back(rootActor);	// Start with the root actor

	// Traverse the subtree of actors starting from the root actor by DFS (Depth-First Search)
	while (!pendingActors.empty())
	{
		// Get the last actor from the stack and validate it
		Actor* actor = pendingActors.back();
		pendingActors.pop_back();

		if (!actor ||
			actor->GetOwner() != scene ||
			actor->IsDestroyed())
		{
			DBG("TransformSubtreeSnapshot::Capture: Encountered an invalid Actor.");
			return false;
		}

		// Get GUID of the actor
		const Guid& actorId = actor->GetGuid();

		// Check if the actor has already been visited (to prevent cycles and duplicates)
		if (!actorId.IsValid() || !visitedActorIds.insert(actorId).second)
		{
			DBG("TransformSubtreeSnapshot::Capture: Invalid, duplicate, or cyclic Actor relationship.");
			return false;
		}

		// Get Transform-family component of the actor and validate it
		Transform* transform = actor->GetComponentByClass<Transform>();

		if (!transform)
		{
			DBG("TransformSubtreeSnapshot::Capture: Actor '%s' has no Transform-family component.", actor->GetName().c_str());
			return false;
		}

		// Capture the Transform-family component state 
		// and store it in the temporary record list
		Record record;
		record.actorId = actorId;
		record.transform = TransformConversion::Capture(*transform);
		capturedRecords.push_back(std::move(record));

		// Get the children of the actor and add them to the stack for processing
		const std::vector<ActorHandle> childHandles = actor->GetChildrenHandles();

		for (auto it = childHandles.rbegin(); it != childHandles.rend(); ++it)
		{
			Actor* child = scene->ResolveActor(*it);

			// Validate the child actor and its relationship to the parent actor(this actor)
			if (!child || child->GetParentHandle() != actor->GetHandle())
			{
				DBG("TransformSubtreeSnapshot::Capture: Invalid child relationship on Actor '%s'.", actor->GetName().c_str());
				return false;
			}

			// Add to the stack for processing in the next iterations
			pendingActors.push_back(child);
		}
	}

	// Capture succeeded
	// Store all result as the snapshot data
	m_rootActorId = rootActor->GetGuid();
	m_records = std::move(capturedRecords);

	return true;
}

bool TransformSubtreeSnapshot::Restore(SceneBase* scene) const
{
	if (!scene || !IsValid()) return false;

	// Temporary struct to hold pending replacements of Transform-family components
	struct PendingReplacement
	{
		Actor* actor = nullptr;
		std::unique_ptr<Transform> transform;
	};

	std::vector<PendingReplacement> replacements;
	replacements.reserve(m_records.size());

	// First phase:
	// Resolve every Actor and construct every replacement Transform.
	// The Scene is not modified during this phase.
	for (const Record& record : m_records)
	{
		// Resolve actor and validate it
		Actor* actor = scene->ResolveActor(record.actorId);

		if (!actor ||
			actor->IsDestroyed() ||
			actor->GetOwner() != scene)
		{
			DBG("TransformSubtreeSnapshot::Restore: Failed to resolve Actor '%s'.", record.actorId.ToString().c_str());
			return false;
		}

		if (!actor->GetComponentByClass<Transform>())
		{
			DBG("TransformSubtreeSnapshot::Restore: Actor '%s' has no Transform-family component.", actor->GetName().c_str());
			return false;
		}

		// Create a new Transform-family component based on the captured snapshot
		std::unique_ptr<Transform> transform =
			TransformConversion::Create(
				record.transform.sourceKind,
				record.transform
			);

		if (!transform)
		{
			DBG("TransformSubtreeSnapshot::Restore: Failed to reconstruct Transform for Actor '%s'.", actor->GetName().c_str());
			return false;
		}

		// Store the actor and its replacement Transform for the second phase
		PendingReplacement replacement;
		replacement.actor = actor;
		replacement.transform = std::move(transform);

		replacements.push_back(std::move(replacement));
	}

	// Second phase:
	// Commit replacements after every Actor and Transform has been validated.
	for (auto& replacement : replacements)
	{
		if (!replacement.actor->ReplaceTransformComponent(std::move(replacement.transform)))
		{
			DBG("TransformSubtreeSnapshot::Restore: Failed to replace Transform on Actor '%s'.", replacement.actor->GetName().c_str());
			return false;
		}
	}

	return true;
}

void TransformSubtreeSnapshot::Clear()
{
	m_rootActorId = {};
	m_records.clear();
}
