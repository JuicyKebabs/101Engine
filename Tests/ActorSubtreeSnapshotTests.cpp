#include "Engine/Actor/ActorFactory.h"
#include "Engine/Scene/ActorSubtreeSnapshot.h"
#include "Engine/Scene/SceneBase.h"

#include <iostream>
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

	Actor* AddRoot(SceneBase& scene, const char* name)
	{
		return scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, name)));
	}

	Actor* AddChild(SceneBase& scene, Actor* parent, const char* name)
	{
		return scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, name)),
			parent->GetHandle());
	}

	void TestCaptureStoresSubtreeInParentFirstOrder()
	{
		SceneBase scene;
		Actor* externalParent = AddRoot(scene, "ExternalParent");
		Actor* root = AddChild(scene, externalParent, "Root");
		Actor* childA = AddChild(scene, root, "ChildA");
		Actor* grandChild = AddChild(scene, childA, "GrandChild");
		Actor* childB = AddChild(scene, root, "ChildB");

		ActorSubtreeSnapshot snapshot;
		Check(snapshot.Capture(root, &scene),
			"Capture accepts a valid Actor subtree");
		Check(snapshot.IsValid(),
			"A successful capture produces a valid snapshot");
		Check(snapshot.GetRootActorId() == root->GetGuid(),
			"Snapshot preserves the root Actor Guid");

		const auto& records = snapshot.GetActorRecords();
		Check(records.size() == 4,
			"Capture includes the root and all descendants only");

		if (records.size() == 4)
		{
			Check(records[0]["actorId"] == root->GetGuid().ToString() &&
				records[1]["actorId"] == childA->GetGuid().ToString() &&
				records[2]["actorId"] == grandChild->GetGuid().ToString() &&
				records[3]["actorId"] == childB->GetGuid().ToString(),
				"Capture preserves parent-first depth-first hierarchy order");

			Check(records[0]["parentId"] == externalParent->GetGuid().ToString(),
				"Subtree root preserves a parent outside the snapshot");
			Check(records[1]["parentId"] == root->GetGuid().ToString() &&
				records[2]["parentId"] == childA->GetGuid().ToString() &&
				records[3]["parentId"] == root->GetGuid().ToString(),
				"Descendant records preserve their parent Guid relationships");
		}
 
		bool containsExternalParent = false;
		for (const auto& record : records)
		{
			if (record["actorId"] == externalParent->GetGuid().ToString())
			{
				containsExternalParent = true;
				break;
			}
		}

		Check(!containsExternalParent,
			"Capture excludes Actors above the selected subtree root");
	}

	void TestRecaptureReplacesPreviousState()
	{
		SceneBase scene;
		Actor* firstRoot = AddRoot(scene, "FirstRoot");
		AddChild(scene, firstRoot, "FirstChild");
		Actor* secondRoot = AddRoot(scene, "SecondRoot");

		ActorSubtreeSnapshot snapshot;
		Check(snapshot.Capture(firstRoot, &scene) &&
			snapshot.GetActorRecords().size() == 2,
			"Initial capture stores the first subtree");
		Check(snapshot.Capture(secondRoot, &scene),
			"A snapshot can capture another subtree");
		Check(snapshot.GetRootActorId() == secondRoot->GetGuid() &&
			snapshot.GetActorRecords().size() == 1 &&
			snapshot.GetActorRecords()[0]["actorId"] == secondRoot->GetGuid().ToString(),
			"Recapture replaces all previous snapshot state");
	}

	void TestFailedCaptureLeavesSnapshotEmpty()
	{
		SceneBase scene;
		SceneBase otherScene;
		Actor* actor = AddRoot(scene, "Actor");

		ActorSubtreeSnapshot snapshot;
		Check(snapshot.Capture(actor, &scene),
			"Valid state exists before testing capture failure");
		Check(!snapshot.Capture(actor, &otherScene),
			"Capture rejects an Actor owned by another Scene");
		Check(!snapshot.IsValid() && snapshot.GetActorRecords().empty(),
			"Failed capture clears the previous snapshot state");

		Check(!snapshot.Capture(nullptr, &scene),
			"Capture rejects a null root Actor");
		Check(!snapshot.Capture(actor, nullptr),
			"Capture rejects a null Scene");

		scene.RemoveActor(actor);
		Check(!snapshot.Capture(actor, &scene),
			"Capture rejects an Actor pending destruction");
	}
}

int main()
{
	TestCaptureStoresSubtreeInParentFirstOrder();
	TestRecaptureReplacesPreviousState();
	TestFailedCaptureLeavesSnapshotEmpty();

	if (g_failures == 0)
	{
		std::cout << "All ActorSubtreeSnapshot tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " ActorSubtreeSnapshot test(s) failed.\n";
	return 1;
}
