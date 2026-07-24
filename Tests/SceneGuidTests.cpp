#include "Engine/Actor/ActorFactory.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/SceneBase.h"

#include <iostream>
#include <memory>
#include <string>

namespace
{
	int g_failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (condition)
		{
			std::cout << "[PASS] " << name << '\n';
			return;
		}

		std::cerr << "[FAIL] " << name << '\n';
		++g_failures;
	}

	std::unique_ptr<Actor> RestoreEmpty(const std::string& name, const Guid& guid)
	{
		return ActorFactory::RestoreEmptyActor(
			Actor::InitDesc(true, TAG_NONE, name),
			guid);
	}

	void TestGuidRegistrationAndResolution()
	{
		SceneBase scene;
		const Guid guid = GuidGenerator::Generate();
		Actor* actor = scene.AddRootActor(RestoreEmpty("Root", guid));

		Check(actor != nullptr, "Scene accepts an actor with a valid Guid");
		Check(scene.ResolveActor(guid) == actor, "Guid resolves to the registered actor");
		Check(scene.FindActorHandle(guid) == actor->GetHandle(),
			"Guid lookup returns the actor's runtime handle");
		Check(scene.ResolveActor(actor->GetHandle()) == scene.ResolveActor(guid),
			"Guid and ActorHandle resolve to the same actor");
	}

	void TestDuplicateGuidRejection()
	{
		SceneBase scene;
		const Guid guid = GuidGenerator::Generate();
		Actor* first = scene.AddRootActor(RestoreEmpty("First", guid));
		Actor* duplicate = scene.AddRootActor(RestoreEmpty("Duplicate", guid));

		Check(first != nullptr, "First occurrence of a Guid is accepted");
		Check(duplicate == nullptr, "Duplicate Guid registration is rejected");
		Check(scene.GetAllActors().size() == 1,
			"Rejected duplicate does not enter the ActorPool");
		Check(scene.ResolveActor(guid) == first,
			"Duplicate rejection preserves the original Guid mapping");
	}

	void TestGuidRemovalAndSlotReuse()
	{
		SceneBase scene;
		const Guid oldGuid = GuidGenerator::Generate();
		Actor* oldActor = scene.AddRootActor(RestoreEmpty("Old", oldGuid));
		const ActorHandle oldHandle = oldActor->GetHandle();

		scene.RemoveActor(oldActor);
		Check(scene.ResolveActor(oldGuid) == oldActor,
			"Pending-destroy actor remains resolvable by Guid");

		scene.LateUpdate(0.0f);
		Check(scene.ResolveActor(oldGuid) == nullptr,
			"Collected actor is removed from the Guid index");
		Check(scene.FindActorHandle(oldGuid).IsNull(),
			"Collected Guid no longer returns a handle");

		const Guid newGuid = GuidGenerator::Generate();
		Actor* replacement = scene.AddRootActor(RestoreEmpty("Replacement", newGuid));
		Check(replacement != nullptr && replacement->GetHandle().index == oldHandle.index,
			"New actor can reuse the collected runtime slot");
		Check(replacement && replacement->GetHandle().generation == oldHandle.generation + 1,
			"Reused runtime slot advances generation");
		Check(scene.ResolveActor(oldGuid) == nullptr,
			"Old Guid does not resolve to the replacement actor");
		Check(scene.ResolveActor(newGuid) == replacement,
			"Replacement Guid resolves to the replacement actor");
	}

	void TestChildGuidRegistrationAndDestroyedParentRejection()
	{
		SceneBase scene;
		const Guid parentGuid = GuidGenerator::Generate();
		const Guid childGuid = GuidGenerator::Generate();
		Actor* parent = scene.AddRootActor(RestoreEmpty("Parent", parentGuid));
		Actor* child = scene.AddChildActor(
			RestoreEmpty("Child", childGuid),
			parent->GetHandle());

		Check(child != nullptr, "Child actor with a valid parent is registered");
		Check(scene.ResolveActor(childGuid) == child,
			"Child actor is registered in the Guid index");
		Check(child && child->GetParent() == parent,
			"Child actor resolves its registered parent");

		scene.RemoveActor(parent);
		const Guid rejectedGuid = GuidGenerator::Generate();
		Actor* rejected = scene.AddChildActor(
			RestoreEmpty("Rejected", rejectedGuid),
			parent->GetHandle());
		Check(rejected == nullptr, "Child registration rejects a parent pending destruction");
		Check(scene.ResolveActor(rejectedGuid) == nullptr,
			"Rejected child does not enter the Guid index");
	}
}

int main()
{
	TestGuidRegistrationAndResolution();
	TestDuplicateGuidRejection();
	TestGuidRemovalAndSlotReuse();
	TestChildGuidRegistrationAndDestroyedParentRejection();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " Scene Guid test(s) failed.\n";
		return 1;
	}

	std::cout << "All Scene Guid tests passed.\n";
	return 0;
}
