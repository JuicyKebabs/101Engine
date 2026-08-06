#include "ActorSubtreeRestorer.h"
#include "Engine/Scene/ActorSubtreeSnapshot.h"
#include "Engine/Scene/ActorDeserializer.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Actor/Actor.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Core/Debug/Debug.h"
#include <optional>
#include <unordered_set>
#include <vector>

using json = nlohmann::json;

namespace
{
	// Struct to hold information about an actor that is pending restoration
	struct PendingRestoreActor
	{
		const json* actorJson = nullptr;

		Guid actorId;

		bool hasParent = false;
		Guid parentId;

		std::unique_ptr<Actor> detachedActor;
		Actor* registeredActor = nullptr;
	};

	// Parse a JSON object representing an actor and populate a PendingRestoreActor record
	bool ParseRestoreRecord(
		const json& actorJson,
		PendingRestoreActor& outRecord
	)
	{
		// Validate JSON structure and required fields
		if (!actorJson.is_object())
		{
			DBG("ActorSubtreeRestorer: Actor record must be an object.");
			return false;
		}

		if (!actorJson.contains("actorId") ||
			!actorJson["actorId"].is_string())
		{
			DBG("ActorSubtreeRestorer: Actor record is missing a valid actorId.");
			return false;
		}

		// Parse the actor's GUID from the "actorId" field
		Guid actorId;
		if (!Guid::TryParse(
			actorJson["actorId"].get<std::string>(),
			actorId))
		{
			DBG("ActorSubtreeRestorer: Actor record contains an invalid actorId.");
			return false;
		}

		if (!actorJson.contains("parentId"))
		{
			DBG("ActorSubtreeRestorer: Actor record is missing parentId.");
			return false;
		}

		outRecord.actorJson = &actorJson;
		outRecord.actorId = actorId;

		// Resolve the parent relationship based on the "parentId" field
		if (actorJson["parentId"].is_null())
		{
			outRecord.hasParent = false;
		}
		else if (actorJson["parentId"].is_string())
		{
			Guid parentId;
			if (!Guid::TryParse(
				actorJson["parentId"].get<std::string>(),
				parentId))
			{
				DBG("ActorSubtreeRestorer: Actor record contains an invalid parentId.");
				return false;
			}

			if (parentId == actorId)
			{
				DBG("ActorSubtreeRestorer: Actor cannot be its own parent.");
				return false;
			}

			outRecord.hasParent = true;
			outRecord.parentId = parentId;
		}
		else
		{
			DBG("ActorSubtreeRestorer: parentId must be a Guid string or null.");
			return false;
		}

		return true;
	}

	// Validate the hierarchy of actors to be restored, 
	// ensuring that parent-child relationships are consistent and valid
	bool ValidateRestoreHierarchy(
		const std::vector<PendingRestoreActor>& records,
		const Guid& rootActorId,
		SceneBase* scene
	)
	{
		if (records.empty()) return false;
		if (!scene) return false;

		if (records.front().actorId != rootActorId)
		{
			DBG("ActorSubtreeRestorer: First record does not match the snapshot root Guid.");
			return false;
		}

		std::unordered_set<Guid> allActorIds;

		// Collect all actor IDs and check for duplicates or conflicts with existing actors in the scene
		for (const auto& record : records)
		{
			// Check for duplicate actor IDs in the restoration records
			if (!allActorIds.insert(record.actorId).second)
			{
				DBG("ActorSubtreeRestorer: Duplicate Actor Guid: %s", record.actorId.ToString().c_str());
				return false;
			}

			// Check if the actor ID already exists in the scene
			if (scene->ResolveActor(record.actorId))
			{
				DBG("ActorSubtreeRestorer: Actor Guid already exists in the Scene: %s", record.actorId.ToString().c_str());
				return false;
			}
		}

		const PendingRestoreActor& rootRecord = records.front();

		if (rootRecord.hasParent)
		{
			// Check if the cycle reference is existing in the restoration records
			if (allActorIds.find(rootRecord.parentId) != allActorIds.end())
			{
				DBG("ActorSubtreeRestorer: Snapshot root cannot have a parent inside its own subtree.");
				return false;
			}

			// Get parent of root existing in the scene, out side of this snapshot restoration
			Actor* externalParent = scene->ResolveActor(rootRecord.parentId);

			// Check if the external parent is valid
			if (!externalParent || externalParent->IsDestroyed())
			{
				DBG("ActorSubtreeRestorer: External parent cannot be resolved.");
				return false;
			}
		}

		// Check if the direction of parent-child relationships is consistent with the order of records
		std::unordered_set<Guid> earlierActorIds;
		earlierActorIds.insert(rootRecord.actorId);

		for (size_t i = 1; i < records.size(); ++i)
		{
			const PendingRestoreActor& record = records[i];

			// Check if the actor has a parent
			if (!record.hasParent)
			{
				DBG("ActorSubtreeRestorer: Non-root Actor '%s' has no parent.", record.actorId.ToString().c_str());
				return false;
			}

			// Check if the parent of the actor has already been processed (i.e., appears earlier in the records)
			if (earlierActorIds.find(record.parentId) == earlierActorIds.end())
			{
				DBG("ActorSubtreeRestorer: Parent of Actor '%s' does not appear before the child.", record.actorId.ToString().c_str());
				return false;
			}

			earlierActorIds.insert(record.actorId);
		}

		return true;
	}
}

Actor* ActorSubtreeRestorer::Restore(
	const ActorSubtreeSnapshot& snapshot,
	SceneBase* scene
)
{
	if (!scene)
	{
		DBG("ActorSubtreeRestorer::Restore: Scene is null.");
		return nullptr;
	}

	if (!snapshot.IsValid())
	{
		DBG("ActorSubtreeRestorer::Restore: Snapshot is invalid.");
		return nullptr;
	}

	// Get stored actor records(JSON data) from the snapshot
	const auto& actorRecords = snapshot.GetActorRecords();

	// Prepare a vector to hold pending restore records for each actor
	std::vector<PendingRestoreActor> pendingRecords;
	pendingRecords.reserve(actorRecords.size());

	// List of handles for actors that have been successfully registered
	std::vector<ActorHandle> registeredHandles;
	registeredHandles.reserve(actorRecords.size());

	// Lambda function to rollback registered actors in case of failure
	auto rollback = [&]()-> Actor*
		{
			if (!registeredHandles.empty() && !scene->RollbackRestoredActors(registeredHandles))
			{
				DBG("ActorSubtreeRestorer::Restore: Rollback did not complete successfully.");
			}

			return nullptr;
		};

	// Parse each actor record and populate the pending restore records
	try
	{
		// First phase:
		// Parse every record without modifying the Scene.
		for (const json& actorJson : actorRecords)
		{
			PendingRestoreActor record;

			if (!ParseRestoreRecord(actorJson, record))
			{
				DBG("ActorSubtreeRestorer::Restore: Failed to parse actor record.");
				return rollback();
			}
			pendingRecords.push_back(std::move(record));
		}

		// Validate the hierarchy of the pending restore records
		if (!ValidateRestoreHierarchy(pendingRecords, snapshot.GetRootActorId(), scene))
		{
			return nullptr;
		}

		// Second phase:
		// Create every Actor while they are still detached from the Scene.
		for (auto& record : pendingRecords)
		{
			record.detachedActor = ActorDeserializer::DeserializeActorRecord(
				*record.actorJson,
				record.actorId
			);

			if (!record.detachedActor)
			{
				DBG("ActorSubtreeRestorer::Restore: Failed to deserialize Actor '%s'.", record.actorId.ToString().c_str());
				return nullptr;
			}
		}

		// Third phase:
		// Register every detached Actor as a temporary root.
		for (auto& record : pendingRecords)
		{
			Actor* registered = scene->RegisterRestoredActor(std::move(record.detachedActor));

			if (!registered)
			{// If the registration fails, rollback all previously registered actors and return nullptr
				DBG("ActorSubtreeRestorer::Restore: Failed to register Actor '%s'.", record.actorId.ToString().c_str());
				return rollback();
			}

			// Store the registered actor pointer in the record for later use
			record.registeredActor = registered;
			registeredHandles.push_back(registered->GetHandle());
		}

		// Fourth phase:
		// Restore hierarchy relationships after every Actor can be resolved by Guid.
		for (auto& record : pendingRecords)
		{
			if (!record.hasParent) continue;

			// Get the parent actor from the scene
			Actor* parent = scene->ResolveActor(record.parentId);

			if (!parent)
			{// If the parent cannot be resolved, rollback all previously registered actors and return nullptr
				DBG("ActorSubtreeRestorer::Restore: Failed to resolve parent of Actor '%s'.", record.actorId.ToString().c_str());
				return rollback();
			}

			// Restore the parent-child relationship in the scene
			if (!scene->RestoreParentRelationship(record.registeredActor, parent))
			{// If restoration fails, rollback all previously registered actors and return nullptr
				DBG("ActorSubtreeRestorer::Restore: Failed to restore parent of Actor '%s'.", record.actorId.ToString().c_str());
				return rollback();
			}
		}

		// Fifth phase:
		// Resolve Actor and asset references only after every Actor is registered.
		for (auto& record : pendingRecords)
		{
			// Resolve component references for the registered actor
			for (Component* component : record.registeredActor->GetAllComponents())
			{
				if (!component || component->IsDestroyed()) continue;

				// Resolve references
				if (!component->ResolveReferences(*scene))
				{// If the component fails to resolve references, rollback all previously registered actors and return nullptr
					const std::type_index typeId(typeid(*component));
					const std::string typeName = ComponentRegistry::Get().GetNameByTypeIndex(typeId);

					DBG(
						"ActorSubtreeRestorer::Restore: Failed to resolve component '%s' on Actor '%s'.",
						typeName.empty()
						? typeId.name()
						: typeName.c_str(),
						record.registeredActor->GetName().c_str()
					);

					return rollback();
				}
			}
		}
		
		// Resolve the restored root actor
		Actor* restoredRoot = scene->ResolveActor(snapshot.GetRootActorId());

		if (!restoredRoot)
		{
			DBG("ActorSubtreeRestorer::Restore: Failed to resolve the restored root Actor.");
			return rollback();
		}

		// Sixth phase:
		// Apply UI constraints only to the restored subtree.
		Canvas* governingCanvas = scene->FindTopmostCanvas(restoredRoot->GetParent());

		// Apply UI hierarchy constraints to the restored subtree based on the governing canvas
		if (!scene->ApplyUIHierarchyConstraints(restoredRoot, governingCanvas))
		{
			DBG("ActorSubtreeRestorer::Restore: Failed to apply UI hierarchy constraints.");
			return rollback();
		}

		return restoredRoot;
	}
	catch (const json::exception& exception)
	{// Catch any JSON parsing exceptions and rollback
		DBG("ActorSubtreeRestorer::Restore: Invalid JSON data: %s", exception.what());

		return rollback();
	}
}
