#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/ActorSubtreeRestorer.h"
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

	void TestRestoreRecreatesSubtreeAndExternalParent()
	{
		SceneBase scene;
		Actor* externalParent = AddRoot(scene, "ExternalParent");
		Actor* root = AddChild(scene, externalParent, "Root");
		Actor* child = AddChild(scene, root, "Child");
		Actor* grandChild = AddChild(scene, child, "GrandChild");

		root->GetComponentByClass<Transform>()->SetLocalPosition({ 4.0f, 5.0f, 6.0f });

		const Guid rootId = root->GetGuid();
		const Guid childId = child->GetGuid();
		const Guid grandChildId = grandChild->GetGuid();

		ActorSubtreeSnapshot snapshot;
		Check(snapshot.Capture(root, &scene),
			"Snapshot captures a subtree before deletion");

		scene.RemoveActor(root);
		scene.LateUpdate(0.0f);

		Check(scene.ResolveActor(rootId) == nullptr &&
			externalParent->GetDirectChildren().empty(),
			"Original subtree is fully removed before restoration");

		Actor* restoredRoot =
			ActorSubtreeRestorer::Restore(snapshot, &scene);
		Actor* restoredChild = scene.ResolveActor(childId);
		Actor* restoredGrandChild = scene.ResolveActor(grandChildId);

		Check(restoredRoot && restoredRoot->GetGuid() == rootId,
			"Restore recreates the root with its original Guid");
		Check(restoredRoot && restoredRoot->GetParent() == externalParent,
			"Restore reconnects the root to its external parent");
		Check(restoredChild && restoredChild->GetParent() == restoredRoot &&
			restoredGrandChild && restoredGrandChild->GetParent() == restoredChild,
			"Restore rebuilds every internal hierarchy relationship");

		Transform* transform = restoredRoot
			? restoredRoot->GetComponentByClass<Transform>()
			: nullptr;
		const Vector3 position = transform
			? transform->GetLocalPosition()
			: Vector3::Zero();
		Check(transform &&
			position.x == 4.0f &&
			position.y == 5.0f &&
			position.z == 6.0f,
			"Restore preserves serialized component parameters");
	}

	void TestGuidCollisionDoesNotModifyScene()
	{
		SceneBase scene;
		Actor* root = AddRoot(scene, "Root");

		ActorSubtreeSnapshot snapshot;
		Check(snapshot.Capture(root, &scene),
			"Snapshot captures an Actor for collision testing");

		const size_t actorCount = scene.GetActorPool().Count();
		Check(ActorSubtreeRestorer::Restore(snapshot, &scene) == nullptr,
			"Restore rejects a Guid already registered in the Scene");
		Check(scene.GetActorPool().Count() == actorCount &&
			scene.ResolveActor(root->GetGuid()) == root,
			"Guid collision leaves the existing Scene unchanged");
	}

	void TestReferenceFailureRollsBackRegisteredActors()
	{
		SceneBase scene;
		Actor* externalParent = AddRoot(scene, "ExternalParent");
		Actor* target = AddRoot(scene, "Target");

		Actor* cameraActor = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::Camera,
				Actor::InitDesc(true, TAG_NONE, "Camera")),
			externalParent->GetHandle());
		cameraActor->GetComponentByClass<Camera>()->SetTargetActor(target);

		const Guid cameraId = cameraActor->GetGuid();
		const ActorHandle originalCameraHandle = cameraActor->GetHandle();

		ActorSubtreeSnapshot snapshot;
		Check(snapshot.Capture(cameraActor, &scene),
			"Snapshot captures an Actor with an external component reference");

		scene.RemoveActor(cameraActor);
		scene.RemoveActor(target);
		scene.LateUpdate(0.0f);

		Check(ActorSubtreeRestorer::Restore(snapshot, &scene) == nullptr,
			"Restore fails when a serialized Actor reference no longer exists");
		Check(scene.ResolveActor(cameraId) == nullptr,
			"Failed restoration removes its temporary Guid mapping");
		Check(externalParent->GetDirectChildren().empty(),
			"Failed restoration removes the temporary external-parent relationship");
		Check(scene.GetActorPool().Count() == 1,
			"Failed restoration discards every temporarily registered Actor");

		Actor* replacement = AddRoot(scene, "Replacement");
		Check(replacement &&
			replacement->GetHandle().index == originalCameraHandle.index &&
			replacement->GetHandle().generation == originalCameraHandle.generation + 2,
			"Rollback releases the temporary slot and advances its generation");
	}
}

int main()
{
	TestRestoreRecreatesSubtreeAndExternalParent();
	TestGuidCollisionDoesNotModifyScene();
	TestReferenceFailureRollsBackRegisteredActors();

	if (g_failures == 0)
	{
		std::cout << "All ActorSubtreeRestorer tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " ActorSubtreeRestorer test(s) failed.\n";
	return 1;
}
