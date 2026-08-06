#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/TransformSubtreeSnapshot.h"

#include <cmath>
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

	bool NearlyEqual(const Quaternion& lhs, const Quaternion& rhs)
	{
		return NearlyEqual(lhs.x, rhs.x) &&
			NearlyEqual(lhs.y, rhs.y) &&
			NearlyEqual(lhs.z, rhs.z) &&
			NearlyEqual(lhs.w, rhs.w);
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

	void TestRestoresTransformSubtreeWithoutRecreatingActors()
	{
		SceneBase scene;
		Actor* root = AddRoot(scene, "Root");
		Actor* child = AddChild(scene, root, "Child");
		Actor* grandChild = AddChild(scene, child, "GrandChild");

		const ActorHandle rootHandle = root->GetHandle();
		const ActorHandle childHandle = child->GetHandle();
		const ActorHandle grandChildHandle = grandChild->GetHandle();

		Transform* rootTransform = root->GetComponentByClass<Transform>();
		Transform* childTransform = child->GetComponentByClass<Transform>();
		Transform* grandChildTransform = grandChild->GetComponentByClass<Transform>();

		const Vector3 rootPosition(1.0f, 2.0f, 3.0f);
		const Quaternion childRotation = Quaternion::CreateFromEulerDeg(Vector3(10.0f, 20.0f, 30.0f));
		const Vector3 grandChildScale(2.0f, 3.0f, 4.0f);

		rootTransform->SetLocalPosition(rootPosition);
		childTransform->SetLocalRotationQuat(childRotation);
		grandChildTransform->SetLocalScale(grandChildScale);

		TransformSubtreeSnapshot snapshot;
		Check(snapshot.Capture(root, &scene),
			"Capture accepts a valid Transform subtree");

		rootTransform->SetLocalPosition(Vector3(11.0f, 12.0f, 13.0f));
		childTransform->SetLocalRotationQuat(Quaternion::Identity());
		grandChildTransform->SetLocalScale(Vector3::One());

		Check(snapshot.Restore(&scene),
			"Restore replaces every Transform in the captured subtree");
		Check(scene.ResolveActor(rootHandle) == root &&
			scene.ResolveActor(childHandle) == child &&
			scene.ResolveActor(grandChildHandle) == grandChild,
			"Restore preserves Actor pointers and handles");

		rootTransform = root->GetComponentByClass<Transform>();
		childTransform = child->GetComponentByClass<Transform>();
		grandChildTransform = grandChild->GetComponentByClass<Transform>();

		Check(NearlyEqual(rootTransform->GetLocalPosition(), rootPosition),
			"Restore recovers the root local position");
		Check(NearlyEqual(childTransform->GetLocalRotationQuat(), childRotation),
			"Restore recovers a descendant local rotation");
		Check(NearlyEqual(grandChildTransform->GetLocalScale(), grandChildScale),
			"Restore recovers a deep descendant local scale");
	}

	void TestRestoresExactRectTransformStateAndConcreteType()
	{
		SceneBase scene;
		Actor* canvas = AddCanvas(scene, "Canvas");
		Actor* actor = AddChild(scene, canvas, "UIActor");

		RectTransform* rectTransform = actor->GetComponentByClass<RectTransform>();
		Check(rectTransform != nullptr,
			"An Actor under a Canvas receives a RectTransform");
		if (!rectTransform) return;

		const Vector3 hiddenLocalPosition(0.0f, 0.0f, 7.0f);
		const Quaternion localRotation = Quaternion::CreateFromEulerDeg(Vector3(0.0f, 0.0f, 25.0f));
		const Vector3 localScale(1.5f, 2.0f, 1.0f);
		const Vector2 anchoredPosition(120.0f, -45.0f);
		const Vector2 pivot(0.2f, 0.8f);
		const Vector2 size(320.0f, 180.0f);

		rectTransform->SetLocalPosition(hiddenLocalPosition);
		rectTransform->SetLocalRotationQuat(localRotation);
		rectTransform->SetLocalScale(localScale);
		rectTransform->SetAnchorMode(AnchorMode::BottomRight);
		rectTransform->SetAnchoredPosition(anchoredPosition);
		rectTransform->SetPivot(pivot);
		rectTransform->SetSizeDelta(size);

		TransformSubtreeSnapshot snapshot;
		Check(snapshot.Capture(actor, &scene),
			"Capture stores RectTransform-specific state");

		Check(scene.ReparentActor(actor, nullptr),
			"Reparenting outside the Canvas converts RectTransform to Transform");
		Check(actor->GetComponentByClass<RectTransform>() == nullptr,
			"The Actor no longer has a RectTransform before restore");

		Check(snapshot.Restore(&scene),
			"Restore reconstructs the captured RectTransform type");

		rectTransform = actor->GetComponentByClass<RectTransform>();
		Check(rectTransform != nullptr,
			"Restore recovers the concrete RectTransform type");
		if (!rectTransform) return;

		Check(NearlyEqual(rectTransform->GetLocalPosition(), Vector3::Zero()) &&
			NearlyEqual(rectTransform->GetLocalRotationQuat(), localRotation) &&
			NearlyEqual(rectTransform->GetLocalScale(), localScale),
			"Restore canonicalizes RectTransform position and recovers rotation and scale");
		Check(rectTransform->GetAnchorMode() == AnchorMode::BottomRight &&
			NearlyEqual(rectTransform->GetAnchoredPosition(), anchoredPosition) &&
			NearlyEqual(rectTransform->GetPivot(), pivot) &&
			NearlyEqual(rectTransform->GetSize(), size),
			"Restore recovers Anchor, AnchoredPosition, Pivot, and Size");
	}

	void TestRecaptureAndFailureBehavior()
	{
		SceneBase scene;
		SceneBase otherScene;
		Actor* first = AddRoot(scene, "First");
		Actor* second = AddRoot(scene, "Second");

		TransformSubtreeSnapshot snapshot;
		Check(snapshot.Capture(first, &scene) &&
			snapshot.GetRootActorId() == first->GetGuid(),
			"Initial capture stores the first root Actor");
		Check(snapshot.Capture(second, &scene) &&
			snapshot.GetRootActorId() == second->GetGuid(),
			"Recapture replaces the previous snapshot");

		Check(!snapshot.Capture(second, &otherScene) && !snapshot.IsValid(),
			"Failed recapture clears previous snapshot state");
		Check(!snapshot.Capture(nullptr, &scene) &&
			!snapshot.Capture(first, nullptr),
			"Capture rejects null inputs");
		Check(!snapshot.Restore(nullptr),
			"Restore rejects a null Scene");
	}

	void TestFailedRestoreDoesNotPartiallyReplaceEarlierActors()
	{
		SceneBase scene;
		Actor* root = AddRoot(scene, "Root");
		Actor* child = AddChild(scene, root, "Child");

		Transform* rootTransform = root->GetComponentByClass<Transform>();
		rootTransform->SetLocalPosition(Vector3(1.0f, 2.0f, 3.0f));

		TransformSubtreeSnapshot snapshot;
		Check(snapshot.Capture(root, &scene),
			"Capture succeeds before testing failed restore");

		const Vector3 changedPosition(20.0f, 30.0f, 40.0f);
		rootTransform->SetLocalPosition(changedPosition);
		scene.RemoveActor(child);

		Check(!snapshot.Restore(&scene),
			"Restore fails when a captured Actor is pending destruction");
		Check(root->GetComponentByClass<Transform>() == rootTransform &&
			NearlyEqual(rootTransform->GetLocalPosition(), changedPosition),
			"Validation failure leaves earlier Actors completely untouched");
	}
}

int main()
{
	TestRestoresTransformSubtreeWithoutRecreatingActors();
	TestRestoresExactRectTransformStateAndConcreteType();
	TestRecaptureAndFailureBehavior();
	TestFailedRestoreDoesNotPartiallyReplaceEarlierActors();

	if (g_failures == 0)
	{
		std::cout << "All TransformSubtreeSnapshot tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " TransformSubtreeSnapshot test(s) failed.\n";
	return 1;
}
