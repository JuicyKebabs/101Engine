#include "Command/EditorCommandHistory.h"
#include "Command/ReparentActorCommand.h"

#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"

#include <cmath>
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

	bool NearlyEqual(float lhs, float rhs)
	{
		return std::abs(lhs - rhs) <= 0.0001f;
	}

	bool NearlyEqual(const Vector2& lhs, const Vector2& rhs)
	{
		return NearlyEqual(lhs.x, rhs.x) &&
			NearlyEqual(lhs.y, rhs.y);
	}

	bool NearlyEqual(const Vector3& lhs, const Vector3& rhs)
	{
		return NearlyEqual(lhs.x, rhs.x) &&
			NearlyEqual(lhs.y, rhs.y) &&
			NearlyEqual(lhs.z, rhs.z);
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

	Actor* AddCanvas(SceneBase& scene, const char* name)
	{
		return scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, name)));
	}

	void TestReparentUndoRedoPreservesActorIdentityAndState()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* oldParent = AddRoot(scene, "OldParent");
		Actor* newParent = AddRoot(scene, "NewParent");
		Actor* actor = AddChild(scene, oldParent, "Actor");
		Actor* child = AddChild(scene, actor, "Child");

		const ActorHandle actorHandle = actor->GetHandle();
		const ActorHandle childHandle = child->GetHandle();
		const Vector3 actorPosition(1.0f, 2.0f, 3.0f);
		const Vector3 childScale(2.0f, 3.0f, 4.0f);

		actor->GetComponentByClass<Transform>()->SetLocalPosition(actorPosition);
		child->GetComponentByClass<Transform>()->SetLocalScale(childScale);

		Check(history.Execute(
			std::make_unique<ReparentActorCommand>(
				&scene,
				actor->GetGuid(),
				newParent->GetGuid())),
			"Execute reparents an Actor through command history");
		Check(actor->GetParent() == newParent &&
			history.GetUndoCount() == 1 &&
			history.GetRedoCount() == 0,
			"Successful Execute updates the hierarchy and history");
		Check(scene.ResolveActor(actorHandle) == actor &&
			scene.ResolveActor(childHandle) == child,
			"Execute preserves Actor pointers and handles for the subtree");

		actor->GetComponentByClass<Transform>()->SetLocalPosition(
			Vector3(10.0f, 20.0f, 30.0f));
		child->GetComponentByClass<Transform>()->SetLocalScale(Vector3::One());

		Check(history.Undo(),
			"Undo restores the previous parent hierarchy");
		Check(actor->GetParent() == oldParent,
			"Undo restores the old parent");
		Check(NearlyEqual(
			actor->GetComponentByClass<Transform>()->GetLocalPosition(),
			actorPosition) &&
			NearlyEqual(
				child->GetComponentByClass<Transform>()->GetLocalScale(),
				childScale),
			"Undo restores the complete before-snapshot subtree state");

		actor->GetComponentByClass<Transform>()->SetLocalPosition(
			Vector3(40.0f, 50.0f, 60.0f));

		Check(history.Redo(),
			"Redo restores the new parent hierarchy");
		Check(actor->GetParent() == newParent,
			"Redo restores the new parent");
		Check(NearlyEqual(
			actor->GetComponentByClass<Transform>()->GetLocalPosition(),
			actorPosition),
			"Redo restores the exact after-snapshot state");
		Check(scene.ResolveActor(actorHandle) == actor &&
			scene.ResolveActor(childHandle) == child,
			"Undo and Redo never recreate Actors");
	}

	void TestCanvasBoundaryRestoresTransformKindsAndRectState()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* canvas = AddCanvas(scene, "Canvas");
		Actor* actor = AddChild(scene, canvas, "UIActor");
		Actor* child = AddChild(scene, actor, "UIChild");

		RectTransform* actorRect = actor->GetComponentByClass<RectTransform>();
		RectTransform* childRect = child->GetComponentByClass<RectTransform>();
		Check(actorRect && childRect,
			"Canvas descendants start with RectTransform components");
		if (!actorRect || !childRect) return;

		const Vector2 anchoredPosition(125.0f, -35.0f);
		const Vector2 pivot(0.25f, 0.75f);
		const Vector2 size(320.0f, 180.0f);
		const Vector2 childSize(64.0f, 48.0f);

		actorRect->SetAnchorMode(AnchorMode::BottomRight);
		actorRect->SetAnchoredPosition(anchoredPosition);
		actorRect->SetPivot(pivot);
		actorRect->SetSizeDelta(size);
		childRect->SetSizeDelta(childSize);

		Check(history.Execute(
			std::make_unique<ReparentActorCommand>(
				&scene,
				actor->GetGuid(),
				Guid{})),
			"Execute moves a Canvas subtree to the scene root");
		Check(actor->GetParent() == nullptr &&
			actor->GetComponentByClass<RectTransform>() == nullptr &&
			child->GetComponentByClass<RectTransform>() == nullptr,
			"Leaving the Canvas converts the complete subtree to Transform");

		Transform* afterTransform = actor->GetComponentByClass<Transform>();
		const Transform3D afterState = afterTransform->GetLocalTransform();
		afterTransform->SetLocalPosition(Vector3(999.0f, 999.0f, 999.0f));

		Check(history.Undo(),
			"Undo moves the subtree back under its Canvas");

		actorRect = actor->GetComponentByClass<RectTransform>();
		childRect = child->GetComponentByClass<RectTransform>();
		Check(actor->GetParent() == canvas && actorRect && childRect,
			"Undo restores RectTransform across the complete subtree");
		Check(actorRect &&
			actorRect->GetAnchorMode() == AnchorMode::BottomRight &&
			NearlyEqual(actorRect->GetAnchoredPosition(), anchoredPosition) &&
			NearlyEqual(actorRect->GetPivot(), pivot) &&
			NearlyEqual(actorRect->GetSize(), size),
			"Undo restores exact RectTransform-specific values");
		Check(childRect && NearlyEqual(childRect->GetSize(), childSize),
			"Undo restores RectTransform state for descendants");

		actorRect->SetAnchoredPosition(Vector2(777.0f, 888.0f));

		Check(history.Redo(),
			"Redo moves the subtree outside the Canvas again");
		afterTransform = actor->GetComponentByClass<Transform>();
		Check(actor->GetParent() == nullptr &&
			afterTransform &&
			actor->GetComponentByClass<RectTransform>() == nullptr,
			"Redo restores the Transform hierarchy produced by Execute");
		Check(afterTransform &&
			NearlyEqual(afterTransform->GetLocalPosition(), afterState.position) &&
			NearlyEqual(afterTransform->GetLocalScale(), afterState.scale),
			"Redo restores the exact after-snapshot Transform values");
	}

	void TestInvalidOperationsDoNotEnterHistory()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* parent = AddRoot(scene, "Parent");
		Actor* actor = AddChild(scene, parent, "Actor");
		Actor* child = AddChild(scene, actor, "Child");

		Check(!history.Execute(
			std::make_unique<ReparentActorCommand>(
				&scene,
				actor->GetGuid(),
				parent->GetGuid())),
			"ReparentActorCommand rejects an unchanged parent");
		Check(history.GetUndoCount() == 0 && history.GetRedoCount() == 0,
			"A no-op does not enter command history");

		Check(!history.Execute(
			std::make_unique<ReparentActorCommand>(
				&scene,
				actor->GetGuid(),
				child->GetGuid())),
			"ReparentActorCommand rejects a hierarchy cycle");
		Check(actor->GetParent() == parent && child->GetParent() == actor,
			"Rejected cycle leaves the hierarchy unchanged");

		Check(!history.Execute(
			std::make_unique<ReparentActorCommand>(
				nullptr,
				actor->GetGuid(),
				Guid{})) &&
			!history.Execute(
				std::make_unique<ReparentActorCommand>(
					&scene,
					Guid{},
					Guid{})),
			"ReparentActorCommand rejects null Scene and Actor inputs");
		Check(history.GetUndoCount() == 0 && history.GetRedoCount() == 0,
			"All rejected operations leave history empty");
	}

	void TestStaleHierarchyAndMissingRollbackParentFailSafely()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* oldParent = AddRoot(scene, "OldParent");
		Actor* newParent = AddRoot(scene, "NewParent");
		Actor* unexpectedParent = AddRoot(scene, "UnexpectedParent");
		Actor* actor = AddChild(scene, oldParent, "Actor");

		auto staleCommand = std::make_unique<ReparentActorCommand>(
			&scene,
			actor->GetGuid(),
			newParent->GetGuid());
		Check(scene.ReparentActor(actor, unexpectedParent),
			"Test setup changes the hierarchy after command construction");
		Check(!history.Execute(std::move(staleCommand)),
			"Execute rejects a hierarchy that changed after command construction");
		Check(actor->GetParent() == unexpectedParent,
			"Rejected stale command does not overwrite the current hierarchy");

		Check(scene.ReparentActor(actor, oldParent),
			"Test setup restores the original hierarchy");
		Check(history.Execute(
			std::make_unique<ReparentActorCommand>(
				&scene,
				actor->GetGuid(),
				newParent->GetGuid())),
			"A valid command executes before testing Undo failure");

		scene.RemoveActor(oldParent);
		Check(!history.Undo(),
			"Undo fails when its stored old parent is pending destruction");
		Check(actor->GetParent() == newParent,
			"Failed Undo leaves the successfully applied hierarchy unchanged");
		Check(history.GetUndoCount() == 1 && history.GetRedoCount() == 0,
			"Failed Undo remains available on the undo stack");
	}

	void TestNestedCanvasRenderModeSurvivesUndoRedo()
	{
		SceneBase scene;
		EditorCommandHistory history;

		Actor* screenCanvasActor = AddCanvas(scene, "ScreenCanvas");
		Canvas* screenCanvas = screenCanvasActor->GetComponentByClass<Canvas>();

		auto worldCanvasOwned = ActorFactory::CreateActor(
			ActorType::Canvas,
			Actor::InitDesc(true, TAG_NONE, "WorldCanvas"));
		Canvas* worldCanvas = worldCanvasOwned->GetComponentByClass<Canvas>();
		Canvas::ParamDesc worldDesc;
		worldDesc.renderMode = CanvasRenderMode::WorldSpace;
		worldCanvas->SetParams(worldDesc);
		Actor* worldCanvasActor = scene.AddRootActor(std::move(worldCanvasOwned));

		Check(screenCanvas && worldCanvas &&
			worldCanvas->GetAuthoredRenderMode() == CanvasRenderMode::WorldSpace &&
			worldCanvas->GetRenderMode() == CanvasRenderMode::WorldSpace,
			"A root World-Space Canvas starts with matching authored and effective modes");

		Check(history.Execute(
			std::make_unique<ReparentActorCommand>(
				&scene,
				worldCanvasActor->GetGuid(),
				screenCanvasActor->GetGuid())),
			"Reparenting nests the World-Space Canvas under a Screen-Space Canvas");
		Check(worldCanvas->GetAuthoredRenderMode() == CanvasRenderMode::WorldSpace &&
			worldCanvas->GetRenderMode() == CanvasRenderMode::ScreenSpace,
			"Nested Canvas inherits the effective mode without losing its authored mode");

		Check(history.Undo(),
			"Undo detaches the nested Canvas");
		Check(worldCanvasActor->GetParent() == nullptr &&
			worldCanvas->GetAuthoredRenderMode() == CanvasRenderMode::WorldSpace &&
			worldCanvas->GetRenderMode() == CanvasRenderMode::WorldSpace,
			"Undo restores the Canvas effective mode from its authored mode");

		Check(history.Redo(),
			"Redo nests the Canvas again");
		Check(worldCanvas->GetAuthoredRenderMode() == CanvasRenderMode::WorldSpace &&
			worldCanvas->GetRenderMode() == CanvasRenderMode::ScreenSpace,
			"Redo reapplies inherited mode while preserving authored mode");
	}
}

int main()
{
	TestReparentUndoRedoPreservesActorIdentityAndState();
	TestCanvasBoundaryRestoresTransformKindsAndRectState();
	TestInvalidOperationsDoNotEnterHistory();
	TestStaleHierarchyAndMissingRollbackParentFailSafely();
	TestNestedCanvasRenderModeSurvivesUndoRedo();

	if (g_failures == 0)
	{
		std::cout << "All ReparentActorCommand tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " ReparentActorCommand test(s) failed.\n";
	return 1;
}
