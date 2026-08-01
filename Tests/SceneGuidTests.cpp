#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/SceneBase.h"

#include <algorithm>
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

	bool HasNormalTransform(Actor* actor)
	{
		if (!actor) return false;

		Transform* transform =
			actor->GetComponentByClass<Transform>();

		return transform &&
			dynamic_cast<RectTransform*>(transform) == nullptr;
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

	void TestReparentActor()
	{
		SceneBase scene;
		Actor* firstParent = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "FirstParent")));
		Actor* secondParent = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "SecondParent")));
		Actor* child = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Child")));

		Check(scene.ReparentActor(child, firstParent),
			"ReparentActor moves a root Actor below a parent");
		Check(child->GetParent() == firstParent,
			"ReparentActor updates the child's parent");
		Check(firstParent->GetDirectChildren().size() == 1 &&
			firstParent->GetDirectChildren().front() == child,
			"ReparentActor adds the child to the new parent");

		Check(scene.ReparentActor(child, secondParent),
			"ReparentActor moves a child between parents");
		Check(child->GetParent() == secondParent &&
			firstParent->GetDirectChildren().empty(),
			"ReparentActor detaches the child from its old parent");

		Check(scene.ReparentActor(child, secondParent),
			"ReparentActor accepts an unchanged parent");
		const auto secondChildren = secondParent->GetChildrenHandles();
		Check(std::count(
			secondChildren.begin(),
			secondChildren.end(),
			child->GetHandle()) == 1,
			"Unchanged reparenting does not duplicate child handles");

		Check(scene.ReparentActor(child, nullptr),
			"ReparentActor moves a child to the scene root");
		Check(child->GetParent() == nullptr &&
			child->GetParentHandle().IsNull() &&
			secondParent->GetDirectChildren().empty(),
			"Root reparenting clears both sides of the relationship");
	}

	void TestReparentRejectsCycles()
	{
		SceneBase scene;
		Actor* root = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Root")));
		Actor* child = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Child")),
			root->GetHandle());
		Actor* grandChild = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "GrandChild")),
			child->GetHandle());

		Check(!scene.ReparentActor(root, root),
			"ReparentActor rejects self-parenting");
		Check(!scene.ReparentActor(root, child),
			"ReparentActor rejects parenting below a direct child");
		Check(!scene.ReparentActor(root, grandChild),
			"ReparentActor rejects parenting below a descendant");
		Check(root->GetParent() == nullptr &&
			child->GetParent() == root &&
			grandChild->GetParent() == child,
			"Rejected cycles leave the original hierarchy unchanged");
	}

	void TestReparentRejectsInvalidActors()
	{
		SceneBase scene;
		SceneBase otherScene;
		Actor* actor = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Actor")));
		Actor* originalParent = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "OriginalParent")));
		Actor* destroyedParent = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "DestroyedParent")));
		Actor* foreignParent = otherScene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "ForeignParent")));

		Check(scene.ReparentActor(actor, originalParent),
			"Reparent test establishes its original hierarchy");

		scene.RemoveActor(destroyedParent);
		Check(!scene.ReparentActor(actor, destroyedParent),
			"ReparentActor rejects a parent pending destruction");
		Check(!scene.ReparentActor(actor, foreignParent),
			"ReparentActor rejects a parent from another Scene");
		Check(actor->GetParent() == originalParent,
			"Rejected parents preserve the original relationship");

		scene.RemoveActor(actor);
		Check(!scene.ReparentActor(actor, nullptr),
			"ReparentActor rejects an Actor pending destruction");
		Check(actor->GetParent() == originalParent,
			"Rejected destroyed Actor reparenting preserves its parent");
	}

	void TestTopmostCanvasControlsNestedCanvases()
	{
		SceneBase scene;
		Actor* rootCanvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "RootCanvas")));
		Actor* container = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Container")),
			rootCanvasActor->GetHandle());
		Actor* nestedCanvasActor = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "NestedCanvas")),
			container->GetHandle());
		Actor* deepCanvasActor = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "DeepCanvas")),
			nestedCanvasActor->GetHandle());

		Canvas* rootCanvas =
			rootCanvasActor->GetComponentByClass<Canvas>();
		Canvas* nestedCanvas =
			nestedCanvasActor->GetComponentByClass<Canvas>();
		Canvas* deepCanvas =
			deepCanvasActor->GetComponentByClass<Canvas>();

		Check(scene.SetCanvasRenderMode(
			rootCanvas,
			CanvasRenderMode::WorldSpace),
			"Topmost Canvas accepts a WorldSpace mode change");
		Check(rootCanvas->GetRenderMode() ==
				CanvasRenderMode::WorldSpace &&
			nestedCanvas->GetRenderMode() ==
				CanvasRenderMode::WorldSpace &&
			deepCanvas->GetRenderMode() ==
				CanvasRenderMode::WorldSpace,
			"Topmost Canvas mode propagates through its full subtree");

		Check(!scene.SetCanvasRenderMode(
			nestedCanvas,
			CanvasRenderMode::ScreenSpace),
			"Nested Canvas rejects a direct render mode change");
		Check(nestedCanvas->GetRenderMode() ==
				CanvasRenderMode::WorldSpace &&
			deepCanvas->GetRenderMode() ==
				CanvasRenderMode::WorldSpace,
			"Rejected nested Canvas change preserves inherited modes");

		Check(scene.SetCanvasRenderMode(
			rootCanvas,
			CanvasRenderMode::ScreenSpace),
			"Topmost Canvas accepts a ScreenSpace mode change");
		Check(nestedCanvas->GetRenderMode() ==
				CanvasRenderMode::ScreenSpace &&
			deepCanvas->GetRenderMode() ==
				CanvasRenderMode::ScreenSpace,
			"ScreenSpace mode propagates to nested Canvases");
	}

	void TestCanvasModeSynchronizesOnRegistrationAndReparent()
	{
		SceneBase scene;
		Actor* worldCanvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "WorldCanvas")));
		Canvas* worldCanvas =
			worldCanvasActor->GetComponentByClass<Canvas>();
		Check(scene.SetCanvasRenderMode(
			worldCanvas,
			CanvasRenderMode::WorldSpace),
			"Registration test configures its topmost Canvas");

		Actor* nestedActor = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "RegisteredNested")),
			worldCanvasActor->GetHandle());
		Canvas* nestedCanvas =
			nestedActor->GetComponentByClass<Canvas>();
		Check(nestedCanvas->GetRenderMode() ==
				CanvasRenderMode::WorldSpace,
			"Child registration inherits the topmost Canvas mode");

		Actor* screenCanvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "ScreenCanvas")));
		Canvas* screenCanvas =
			screenCanvasActor->GetComponentByClass<Canvas>();

		Check(scene.ReparentActor(
			nestedActor,
			screenCanvasActor),
			"Nested Canvas can move below another topmost Canvas");
		Check(nestedCanvas->GetRenderMode() ==
				CanvasRenderMode::ScreenSpace,
			"Reparenting synchronizes the moved Canvas subtree");

		Check(scene.ReparentActor(nestedActor, nullptr),
			"Nested Canvas can become a topmost Canvas");
		Check(scene.SetCanvasRenderMode(
			nestedCanvas,
			CanvasRenderMode::WorldSpace),
			"Detached Canvas can configure its own render mode");
		Check(nestedCanvas->GetRenderMode() ==
				CanvasRenderMode::WorldSpace &&
			screenCanvas->GetRenderMode() ==
				CanvasRenderMode::ScreenSpace,
			"Detached Canvas no longer affects its former hierarchy");
	}

	void TestCanvasModeRejectsInvalidSceneOperations()
	{
		SceneBase scene;
		SceneBase otherScene;
		Actor* canvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "Canvas")));
		Actor* foreignCanvasActor = otherScene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "ForeignCanvas")));
		Canvas* canvas =
			canvasActor->GetComponentByClass<Canvas>();
		Canvas* foreignCanvas =
			foreignCanvasActor->GetComponentByClass<Canvas>();

		Check(!scene.SetCanvasRenderMode(
			nullptr,
			CanvasRenderMode::WorldSpace),
			"Canvas mode change rejects a null Canvas");
		Check(!scene.SetCanvasRenderMode(
			canvas,
			CanvasRenderMode::Max),
			"Canvas mode change rejects an invalid mode");
		Check(!scene.SetCanvasRenderMode(
			foreignCanvas,
			CanvasRenderMode::WorldSpace),
			"Canvas mode change rejects a Canvas from another Scene");
		Check(canvas->GetRenderMode() ==
			CanvasRenderMode::ScreenSpace,
			"Rejected Canvas mode changes preserve existing state");

		scene.RemoveActor(canvasActor);
		Check(!scene.SetCanvasRenderMode(
			canvas,
			CanvasRenderMode::WorldSpace),
			"Canvas mode change rejects an owner pending destruction");
		Check(canvas->GetRenderMode() ==
			CanvasRenderMode::ScreenSpace,
			"Destroyed-owner rejection preserves the Canvas mode");
	}

	void TestCanvasRenderModeControlsTransformKinds()
	{
		SceneBase scene;
		Actor* canvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "Canvas")));
		Actor* child = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Child")),
			canvasActor->GetHandle());
		Canvas* canvas =
			canvasActor->GetComponentByClass<Canvas>();

		Check(canvasActor->GetComponentByClass<RectTransform>() != nullptr,
			"Topmost ScreenSpace Canvas uses RectTransform");
		Check(child->GetComponentByClass<RectTransform>() != nullptr,
			"Actor registered below a Canvas uses RectTransform");

		Check(scene.SetCanvasRenderMode(
			canvas,
			CanvasRenderMode::WorldSpace),
			"Topmost Canvas changes to WorldSpace");
		Check(HasNormalTransform(canvasActor),
			"Topmost WorldSpace Canvas uses a normal Transform");
		Check(child->GetComponentByClass<RectTransform>() != nullptr,
			"WorldSpace Canvas descendants continue using RectTransform");
		Check(canvasActor->CountComponentFamily(
			ComponentFamily::Transform) == 1 &&
			child->CountComponentFamily(
				ComponentFamily::Transform) == 1,
			"Render mode conversion preserves one Transform-family component");

		Check(scene.SetCanvasRenderMode(
			canvas,
			CanvasRenderMode::ScreenSpace),
			"Topmost Canvas changes back to ScreenSpace");
		Check(canvasActor->GetComponentByClass<RectTransform>() != nullptr,
			"ScreenSpace conversion restores the Canvas RectTransform");
	}

	void TestReparentConvertsEntireTransformSubtree()
	{
		SceneBase scene;
		Actor* canvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "Canvas")));
		Actor* subtreeRoot = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "SubtreeRoot")));
		Actor* subtreeChild = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "SubtreeChild")),
			subtreeRoot->GetHandle());

		Check(HasNormalTransform(subtreeRoot) &&
			HasNormalTransform(subtreeChild),
			"Canvas-external subtree starts with normal Transforms");

		Check(scene.ReparentActor(
			subtreeRoot,
			canvasActor),
			"Subtree can move below a Canvas");
		Check(subtreeRoot->GetComponentByClass<RectTransform>() != nullptr &&
			subtreeChild->GetComponentByClass<RectTransform>() != nullptr,
			"Reparenting below a Canvas converts the entire subtree");

		Check(scene.ReparentActor(
			subtreeRoot,
			nullptr),
			"Subtree can leave its Canvas hierarchy");
		Check(HasNormalTransform(subtreeRoot) &&
			HasNormalTransform(subtreeChild),
			"Leaving a Canvas converts the entire subtree back to Transform");
		Check(subtreeRoot->CountComponentFamily(
			ComponentFamily::Transform) == 1 &&
			subtreeChild->CountComponentFamily(
				ComponentFamily::Transform) == 1,
			"Subtree conversions preserve Transform-family exclusivity");
	}
}

int main()
{
	TestGuidRegistrationAndResolution();
	TestDuplicateGuidRejection();
	TestGuidRemovalAndSlotReuse();
	TestChildGuidRegistrationAndDestroyedParentRejection();
	TestReparentActor();
	TestReparentRejectsCycles();
	TestReparentRejectsInvalidActors();
	TestTopmostCanvasControlsNestedCanvases();
	TestCanvasModeSynchronizesOnRegistrationAndReparent();
	TestCanvasModeRejectsInvalidSceneOperations();
	TestCanvasRenderModeControlsTransformKinds();
	TestReparentConvertsEntireTransformSubtree();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " Scene Guid test(s) failed.\n";
		return 1;
	}

	std::cout << "All Scene Guid tests passed.\n";
	return 0;
}
