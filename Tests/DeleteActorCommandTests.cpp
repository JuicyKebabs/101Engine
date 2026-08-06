#include "Command/DeleteActorCommand.h"
#include "Command/EditorCommandHistory.h"

#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Transform.h"
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

	Actor* AddEmpty(SceneBase& scene, const char* name)
	{
		return scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, name)));
	}

	void TestDeleteUndoRedoRestoresSubtree()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* externalParent = AddEmpty(scene, "ExternalParent");
		Actor* root = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Root")),
			externalParent->GetHandle());
		Actor* child = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Child")),
			root->GetHandle());

		root->GetComponentByClass<Transform>()->SetLocalPosition({ 3.0f, 4.0f, 5.0f });

		const Guid rootId = root->GetGuid();
		const Guid childId = child->GetGuid();

		Check(history.Execute(
			std::make_unique<DeleteActorCommand>(&scene, rootId)),
			"DeleteActorCommand deletes an Actor through command history");
		Check(root->IsDestroyed() && child->IsDestroyed(),
			"DeleteActorCommand marks the complete subtree for destruction");

		Check(!history.Undo(),
			"Undo safely fails before deferred garbage collection");
		Check(history.GetUndoCount() == 1 && history.GetRedoCount() == 0,
			"Failed Undo leaves DeleteActorCommand on the undo stack");

		scene.LateUpdate(0.0f);
		Check(history.Undo(),
			"Undo restores the subtree after garbage collection");

		Actor* restoredRoot = scene.ResolveActor(rootId);
		Actor* restoredChild = scene.ResolveActor(childId);
		Check(restoredRoot && restoredChild,
			"Undo restores every Actor with its original Guid");
		Check(restoredRoot && restoredRoot->GetParent() == externalParent &&
			restoredChild && restoredChild->GetParent() == restoredRoot,
			"Undo restores external and internal hierarchy relationships");

		Transform* restoredTransform = restoredRoot
			? restoredRoot->GetComponentByClass<Transform>()
			: nullptr;
		const Vector3 position = restoredTransform
			? restoredTransform->GetLocalPosition()
			: Vector3::Zero();
		Check(restoredTransform &&
			position.x == 3.0f &&
			position.y == 4.0f &&
			position.z == 5.0f,
			"Undo restores serialized component parameters");

		Check(history.Redo(),
			"Redo deletes the restored subtree again");
		Check(restoredRoot->IsDestroyed() && restoredChild->IsDestroyed(),
			"Redo marks the recreated subtree for destruction");

		scene.LateUpdate(0.0f);
		Check(history.Undo(),
			"DeleteActorCommand supports repeated Undo after Redo");
		Check(scene.ResolveActor(rootId) && scene.ResolveActor(childId),
			"Repeated Undo continues to preserve Actor Guids");
	}

	void TestDeleteUndoReconnectsMainCamera()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* cameraActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Camera,
				Actor::InitDesc(true, ActorTags::MainCamera, "MainCamera")));
		Camera* camera = cameraActor->GetComponentByClass<Camera>();
		const Guid cameraId = cameraActor->GetGuid();

		Check(scene.GetCameraSystem()->SetMainCamera(camera),
			"CameraSystem selects the Actor before command deletion");
		Check(history.Execute(
			std::make_unique<DeleteActorCommand>(&scene, cameraActor->GetGuid())),
			"DeleteActorCommand deletes the main Camera Actor");
		Check(scene.GetCameraSystem()->GetMainCamera() == nullptr,
			"Deleted main Camera becomes unavailable immediately");

		scene.LateUpdate(0.0f);
		Check(history.Undo(),
			"Undo restores the main Camera Actor");

		Actor* restoredActor = scene.ResolveActor(cameraId);
		Camera* restoredCamera = restoredActor
			? restoredActor->GetComponentByClass<Camera>()
			: nullptr;
		Check(restoredCamera &&
			scene.GetCameraSystem()->GetMainCamera() == restoredCamera,
			"CameraSystem reconnects to the restored main Camera automatically");
	}

	void TestInvalidDeleteCommandUsage()
	{
		EditorCommandHistory history;
		SceneBase scene;

		Check(!history.Execute(
			std::make_unique<DeleteActorCommand>(nullptr, Guid{})),
			"DeleteActorCommand rejects a null Scene and Actor");
		Check(!history.Execute(
			std::make_unique<DeleteActorCommand>(&scene, Guid{})),
			"DeleteActorCommand rejects a null Actor");
	}
}

int main()
{
	TestDeleteUndoRedoRestoresSubtree();
	TestDeleteUndoReconnectsMainCamera();
	TestInvalidDeleteCommandUsage();

	if (g_failures == 0)
	{
		std::cout << "All DeleteActorCommand tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " DeleteActorCommand test(s) failed.\n";
	return 1;
}
