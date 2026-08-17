#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Collider.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/RendererComponent.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/TransformConversion.h"
#include "Engine/Core/Serialization/JsonMath.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"
#include "Engine/UI/UIRenderer.h"
#include "nlohmann/json.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>

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

	bool Near(float left, float right, float epsilon = 0.001f)
	{
		return std::fabs(left - right) <= epsilon;
	}

	bool Near(const Vector3& left, const Vector3& right)
	{
		return Near(left.x, right.x) &&
			Near(left.y, right.y) &&
			Near(left.z, right.z);
	}

	bool Near(const Vector2& left, const Vector2& right)
	{
		return Near(left.x, right.x) && Near(left.y, right.y);
	}

	bool Near(const Vector4& left, const Vector4& right)
	{
		return Near(left.x, right.x) &&
			Near(left.y, right.y) &&
			Near(left.z, right.z) &&
			Near(left.w, right.w);
	}

	class TestRendererComponent final : public RendererComponent
	{
	private:
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	CameraLens MakeValidCameraLens()
	{
		CameraLens lens;
		lens.fov = 1.2f;
		lens.width = 1280.0f;
		lens.height = 720.0f;
		lens.nearZ = 0.25f;
		lens.farZ = 500.0f;
		lens.projectionType = PROJECTION_TYPE::PROJECTION_TYPE_ORTHOGRAPHIC;
		return lens;
	}

	void TestComponentContract()
	{
		Check(std::has_virtual_destructor_v<Component>,
			"Component has a virtual destructor");
	}

	void TestTransformRoundTrip()
	{
		Transform source;
		source.SetParams({
			.localPosition = {1.5f, -2.0f, 3.25f},
			.localRotation = Quaternion::CreateFromEulerDeg({15.0f, 30.0f, -20.0f}),
			.localScale = {2.0f, 3.0f, 4.0f},
			.name = "SavedTransform"
		});

		nlohmann::json serialized;
		Check(source.Serialize(serialized), "Transform Serialize succeeds");
		Check(serialized.is_object() &&
			serialized.contains("position") &&
			serialized.contains("rotation") &&
			serialized.contains("scale") &&
			serialized.contains("name"),
			"Transform Serialize writes the complete schema");

		Transform restored;
		Check(restored.Deserialize(serialized), "Transform Deserialize succeeds");
		Check(Near(restored.GetLocalPosition(), source.GetLocalPosition()),
			"Transform position survives a round trip");
		Check(restored.GetLocalRotationQuat().NearEqual(
			source.GetLocalRotationQuat(), 0.001f),
			"Transform rotation survives a round trip");
		Check(Near(restored.GetLocalScale(), source.GetLocalScale()),
			"Transform scale survives a round trip");
		Check(restored.GetName() == "SavedTransform",
			"Transform name survives a round trip");
	}

	void TestInvalidJsonDoesNotPartiallyMutate()
	{
		Transform transform;
		transform.SetParams({
			.localPosition = {7.0f, 8.0f, 9.0f},
			.localRotation = Quaternion::CreateFromEulerDeg({10.0f, 20.0f, 30.0f}),
			.localScale = {2.0f, 2.0f, 2.0f},
			.name = "Original"
		});

		const Vector3 originalPosition = transform.GetLocalPosition();
		const Quaternion originalRotation = transform.GetLocalRotationQuat();
		const Vector3 originalScale = transform.GetLocalScale();
		const std::string originalName = transform.GetName();

		const nlohmann::json invalid = {
			{"position", {100.0f, 200.0f, 300.0f}},
			{"rotation", "invalid"},
			{"scale", {5.0f, 5.0f, 5.0f}},
			{"name", "Mutated"}
		};

		Check(!transform.Deserialize(invalid), "Transform rejects invalid JSON");
		Check(Near(transform.GetLocalPosition(), originalPosition) &&
			transform.GetLocalRotationQuat().NearEqual(originalRotation, 0.001f) &&
			Near(transform.GetLocalScale(), originalScale) &&
			transform.GetName() == originalName,
			"Rejected JSON leaves the Transform unchanged");
	}

	void TestSetParamsPropagatesDirtyToChildren()
	{
		SceneBase scene;
		auto parentOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Parent"));
		Actor* parent = scene.AddRootActor(std::move(parentOwned));
		auto childOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Child"));
		Actor* child = scene.AddChildActor(
			std::move(childOwned), parent->GetHandle());

		parent->GetComponentByClass<Transform>()->SetLocalPosition({1.0f, 0.0f, 0.0f});
		child->GetComponentByClass<Transform>()->SetLocalPosition({2.0f, 0.0f, 0.0f});
		scene.Update(0.0f);
		Check(Near(child->GetComponentByClass<Transform>()->GetWorldPosition(),
			{3.0f, 0.0f, 0.0f}),
			"Initial parent-child world transform is correct");

		Transform::ParamDesc changed;
		changed.localPosition = {5.0f, 0.0f, 0.0f};
		parent->GetComponentByClass<Transform>()->SetParams(changed);
		scene.Update(0.0f);
		Check(Near(child->GetComponentByClass<Transform>()->GetWorldPosition(),
			{7.0f, 0.0f, 0.0f}),
			"SetParams propagates dirty state to child transforms");
	}

	void TestCameraRoundTrip()
	{
		Camera source;
		source.SetName("SavedCamera");
		source.SetFollowMode(CAMERA_FOLLOW_MODE::FOLLOW_MODE_FIXED);
		source.SetRotationMode(CAMERA_ROTATION_MODE::ROTATION_MODE_FIXED);
		source.SetCameraRig({
			.offsetPosition = {1.0f, 2.0f, 3.0f},
			.offsetRotation = Quaternion::CreateFromEulerDeg({10.0f, 20.0f, 30.0f})
		});
		source.SetCameraPose({
			.position = {4.0f, 5.0f, 6.0f},
			.rotation = Quaternion::CreateFromEulerDeg({-5.0f, 15.0f, 25.0f})
		});
		source.SetCameraLens(MakeValidCameraLens());

		nlohmann::json serialized;
		Check(source.Serialize(serialized), "Camera Serialize succeeds");

		Camera restored;
		Check(restored.Deserialize(serialized), "Camera Deserialize succeeds");

		const CameraRig restoredRig = restored.GetCameraRig();
		const CameraPose restoredPose = restored.GetCameraPose();
		const CameraLens restoredLens = restored.GetCameraLens();
		Check(restored.GetName() == source.GetName(),
			"Camera name survives a round trip");
		Check(restored.GetFollowMode() == source.GetFollowMode() &&
			restored.GetRotationMode() == source.GetRotationMode(),
			"Camera modes survive a round trip");
		Check(Near(restoredRig.offsetPosition, source.GetCameraRig().offsetPosition) &&
			restoredRig.offsetRotation.NearEqual(
				source.GetCameraRig().offsetRotation, 0.001f),
			"Camera rig survives a round trip");
		Check(Near(restoredPose.position, source.GetCameraPose().position) &&
			restoredPose.rotation.NearEqual(
				source.GetCameraPose().rotation, 0.001f),
			"Camera pose survives a round trip");
		Check(Near(restoredLens.fov, source.GetCameraLens().fov) &&
			Near(restoredLens.width, source.GetCameraLens().width) &&
			Near(restoredLens.height, source.GetCameraLens().height) &&
			Near(restoredLens.nearZ, source.GetCameraLens().nearZ) &&
			Near(restoredLens.farZ, source.GetCameraLens().farZ) &&
			restoredLens.projectionType == source.GetCameraLens().projectionType,
			"Camera lens survives a round trip");
	}

	void TestInvalidCameraJsonDoesNotPartiallyMutate()
	{
		Camera camera;
		camera.SetName("OriginalCamera");
		camera.SetCameraLens(MakeValidCameraLens());

		nlohmann::json before;
		camera.Serialize(before);

		nlohmann::json invalid = before;
		invalid["name"] = "MutatedCamera";
		invalid["lens"]["farZ"] = 0.1f;

		Check(!camera.Deserialize(invalid), "Camera rejects invalid JSON");

		nlohmann::json after;
		camera.Serialize(after);
		Check(after == before, "Rejected JSON leaves the Camera unchanged");
	}

	void TestCameraReferenceResolution()
	{
		SceneBase scene;
		Actor* target = scene.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Target")));
		Actor* follow = scene.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Follow")));

		Camera source;
		source.SetCameraLens(MakeValidCameraLens());
		source.SetTargetActor(target);
		source.SetFollowTarget(follow);

		nlohmann::json serialized;
		source.Serialize(serialized);

		Camera restored;
		Check(restored.Deserialize(serialized),
			"Camera with Actor references deserializes");
		Check(restored.ResolveReferences(scene),
			"Camera Actor references resolve by Guid");

		nlohmann::json resolvedJson;
		restored.Serialize(resolvedJson);
		Check(resolvedJson["targetActorId"] == target->GetGuid().ToString() &&
			resolvedJson["followActorId"] == follow->GetGuid().ToString(),
			"Resolved Camera preserves both Actor references");

		Check(restored.ResolveReferences(scene),
			"Camera reference resolution can be called repeatedly");
		nlohmann::json repeatedJson;
		restored.Serialize(repeatedJson);
		Check(repeatedJson["targetActorId"] == target->GetGuid().ToString() &&
			repeatedJson["followActorId"] == follow->GetGuid().ToString(),
			"Repeated resolution does not clear resolved references");
	}

	void TestCameraMissingReferenceFails()
	{
		Camera source;
		source.SetCameraLens(MakeValidCameraLens());

		nlohmann::json serialized;
		source.Serialize(serialized);
		serialized["targetActorId"] = GuidGenerator::Generate().ToString();

		Camera restored;
		Check(restored.Deserialize(serialized),
			"Camera accepts a valid unresolved Actor Guid");

		SceneBase scene;
		Check(!restored.ResolveReferences(scene),
			"Camera reports a missing Actor reference");

		nlohmann::json unresolvedJson;
		restored.Serialize(unresolvedJson);
		Check(unresolvedJson["targetActorId"] == serialized["targetActorId"],
			"Failed resolution retains the pending Actor Guid");
	}

	void TestDefaultCameraRoundTrip()
	{
		Camera source;
		nlohmann::json serialized;
		source.Serialize(serialized);

		Camera restored;
		Check(restored.Deserialize(serialized),
			"Default Camera survives its own serialized schema");
	}

	void TestColliderRoundTrip()
	{
		Collider source;
		source.SetParams({
			.localCenter = {1.0f, -2.0f, 3.0f},
			.localRotation = Quaternion::CreateFromEulerDeg({10.0f, 25.0f, -15.0f}),
			.localScale = {2.0f, 3.0f, 4.0f},
			.type = ColliderType::CAPSULE,
			.layer = CollisionLayer::ENEMY,
			.isTrigger = true,
			.name = "SavedCollider"
		});

		nlohmann::json serialized;
		Check(source.Serialize(serialized), "Collider Serialize succeeds");

		Collider restored;
		Check(restored.Deserialize(serialized), "Collider Deserialize succeeds");

		nlohmann::json restoredJson;
		restored.Serialize(restoredJson);
		Vector3 restoredCenter;
		Quaternion restoredRotation;
		Vector3 restoredScale;
		const bool mathReadSucceeded =
			JsonMath::TryRead(restoredJson["center"], restoredCenter) &&
			JsonMath::TryRead(restoredJson["rotation"], restoredRotation) &&
			JsonMath::TryRead(restoredJson["scale"], restoredScale);

		Check(mathReadSucceeded &&
			Near(restoredCenter, {1.0f, -2.0f, 3.0f}) &&
			restoredRotation.NearEqual(
				Quaternion::CreateFromEulerDeg({10.0f, 25.0f, -15.0f}), 0.001f) &&
			Near(restoredScale, {2.0f, 3.0f, 4.0f}) &&
			restored.GetType() == ColliderType::CAPSULE &&
			restored.GetLayer() == CollisionLayer::ENEMY &&
			restored.GetLayerMask() == MakeLayerMask(CollisionLayer::ENEMY) &&
			restored.IsTrigger() &&
			restored.GetName() == "SavedCollider",
			"Collider settings survive a round trip");
	}

	void TestInvalidColliderJsonDoesNotPartiallyMutate()
	{
		Collider collider;
		collider.SetParams({
			.localCenter = {3.0f, 2.0f, 1.0f},
			.localScale = {2.0f, 2.0f, 2.0f},
			.type = ColliderType::BOX,
			.layer = CollisionLayer::PLAYER,
			.name = "OriginalCollider"
		});

		nlohmann::json before;
		collider.Serialize(before);
		nlohmann::json invalid = before;
		invalid["name"] = "MutatedCollider";
		invalid["type"] = 999;

		Check(!collider.Deserialize(invalid), "Collider rejects invalid JSON");

		nlohmann::json after;
		collider.Serialize(after);
		Check(after == before, "Rejected JSON leaves the Collider unchanged");
	}

	void TestColliderValidationBoundaries()
	{
		Collider source;
		nlohmann::json valid;
		source.Serialize(valid);

		Collider restored;

		nlohmann::json invalidLayer = valid;
		invalidLayer["layer"] = static_cast<int>(CollisionLayer::MAX_LAYER);
		Check(!restored.Deserialize(invalidLayer),
			"Collider rejects the MAX_LAYER sentinel");

		nlohmann::json invalidScale = valid;
		invalidScale["scale"] = {1.0f, 0.0f, 1.0f};
		Check(!restored.Deserialize(invalidScale),
			"Collider rejects a zero scale axis");

		nlohmann::json invalidRotation = valid;
		invalidRotation["rotation"] = {0.0f, 0.0f, 0.0f, 0.0f};
		Check(!restored.Deserialize(invalidRotation),
			"Collider rejects a zero-length Quaternion");
	}

	void TestDefaultColliderRoundTrip()
	{
		Collider source;
		nlohmann::json serialized;
		source.Serialize(serialized);

		Collider restored;
		Check(restored.Deserialize(serialized),
			"Default Collider survives its own serialized schema");
	}

	void TestRectTransformRoundTrip()
	{
		RectTransform source;
		static_cast<Transform&>(source).SetParams({
			.localPosition = {1.0f, 2.0f, 3.0f},
			.localRotation = Quaternion::CreateFromEulerDeg({5.0f, 10.0f, 15.0f}),
			.localScale = {2.0f, 2.5f, 3.0f},
			.name = "BaseName"
		});
		source.SetParams({
			.anchorMode = AnchorMode::BottomRight,
			.anchoredPosition = {120.0f, -45.0f},
			.pivot = {0.25f, 0.75f},
			.size = {320.0f, 180.0f},
			.name = "SavedRectTransform"
		});

		nlohmann::json serialized;
		Check(source.Serialize(serialized), "RectTransform Serialize succeeds");

		RectTransform restored;
		Check(restored.Deserialize(serialized), "RectTransform Deserialize succeeds");
		Check(restored.GetName() == "SavedRectTransform" &&
			Near(restored.GetLocalPosition(), Vector3::Zero()) &&
			restored.GetLocalRotationQuat().NearEqual(
				source.GetLocalRotationQuat(), 0.001f) &&
			Near(restored.GetLocalScale(), source.GetLocalScale()) &&
			restored.GetAnchorMode() == source.GetAnchorMode() &&
			Near(restored.GetAnchoredPosition(), source.GetAnchoredPosition()) &&
			Near(restored.GetPivot(), source.GetPivot()) &&
			Near(restored.GetSize(), source.GetSize()),
			"RectTransform round trip canonicalizes position and preserves other settings");
	}

	void TestInvalidRectTransformUiDoesNotMutateBase()
	{
		RectTransform rect;
		static_cast<Transform&>(rect).SetParams({
			.localPosition = {7.0f, 8.0f, 9.0f},
			.name = "OriginalRect"
		});
		rect.SetParams({
			.anchorMode = AnchorMode::TopLeft,
			.anchoredPosition = {10.0f, 20.0f},
			.pivot = {0.5f, 0.5f},
			.size = {100.0f, 50.0f},
			.name = "OriginalRect"
		});

		nlohmann::json before;
		rect.Serialize(before);
		nlohmann::json invalid = before;
		invalid["name"] = "MutatedRect";
		invalid["position"] = {100.0f, 200.0f, 300.0f};
		invalid["pivot"] = {1.5f, 0.5f};

		Check(!rect.Deserialize(invalid),
			"RectTransform rejects invalid UI data");
		nlohmann::json after;
		rect.Serialize(after);
		Check(after == before,
			"Invalid UI data leaves base and UI state unchanged");
	}

	void TestInvalidRectTransformBaseDoesNotMutateUi()
	{
		RectTransform rect;
		rect.SetParams({
			.anchorMode = AnchorMode::MiddleRight,
			.anchoredPosition = {12.0f, 34.0f},
			.pivot = {0.2f, 0.8f},
			.size = {64.0f, 32.0f},
			.name = "OriginalRect"
		});

		nlohmann::json before;
		rect.Serialize(before);
		nlohmann::json invalid = before;
		invalid["anchorMode"] = static_cast<int>(AnchorMode::BottomLeft);
		invalid["anchoredPosition"] = {99.0f, 88.0f};
		invalid["rotation"] = "invalid";

		Check(!rect.Deserialize(invalid),
			"RectTransform rejects invalid base Transform data");
		nlohmann::json after;
		rect.Serialize(after);
		Check(after == before,
			"Invalid base data leaves RectTransform UI state unchanged");
	}

	void TestRectTransformValidationBoundaries()
	{
		RectTransform source;
		nlohmann::json valid;
		source.Serialize(valid);

		RectTransform restored;
		nlohmann::json invalidAnchor = valid;
		invalidAnchor["anchorMode"] = 999;
		Check(!restored.Deserialize(invalidAnchor),
			"RectTransform rejects an invalid AnchorMode");

		nlohmann::json negativeSize = valid;
		negativeSize["size"] = {-1.0f, 1.0f};
		Check(!restored.Deserialize(negativeSize),
			"RectTransform rejects a negative size");

		nlohmann::json zeroSize = valid;
		zeroSize["size"] = {0.0f, 0.0f};
		Check(restored.Deserialize(zeroSize),
			"RectTransform accepts a zero size");
	}

	void TestDefaultRectTransformRoundTrip()
	{
		RectTransform source;
		nlohmann::json serialized;
		source.Serialize(serialized);

		RectTransform restored;
		Check(restored.Deserialize(serialized),
			"Default RectTransform survives its own serialized schema");
	}

	void TestRectTransformInheritsTransformParent()
	{
		SceneBase scene;
		Actor* parent = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "Parent")));
		Canvas* parentCanvas =
			parent->GetComponentByClass<Canvas>();
		Check(scene.SetCanvasScaleMode(
			parentCanvas,
			CanvasScaleMode::ConstantPixelSize),
			"Canvas accepts ConstantPixelSize for Transform inheritance test");
		Check(scene.SetCanvasRenderMode(
			parentCanvas,
			CanvasRenderMode::WorldSpace),
			"WorldSpace Canvas configures a Transform parent for RectTransform");
		Actor* child = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::UI,
				Actor::InitDesc(true, TAG_NONE, "UIChild")),
			parent->GetHandle());

		Transform* parentTransform =
			parent->GetComponentByClass<Transform>();
		RectTransform* childTransform =
			child->GetComponentByClass<RectTransform>();

		parentTransform->SetLocalPosition({10.0f, 20.0f, 30.0f});
		parentTransform->SetLocalScale({2.0f, 2.0f, 2.0f});
		childTransform->SetAnchorMode(AnchorMode::MiddleCenter);
		childTransform->SetAnchoredPosition({3.0f, 4.0f});
		childTransform->SetSizeDelta({100.0f, 50.0f});

		scene.Update(0.0f);

		Check(Near(
			childTransform->GetWorldPosition(),
			Vector3(16.0f, 28.0f, 30.0f)),
			"RectTransform inherits its Transform parent's pose");
		Check(Near(
			childTransform->GetWorldScale(),
			Vector3(2.0f, 2.0f, 2.0f)),
			"RectTransform size is excluded from its inherited Transform scale");
		Check(Near(
			childTransform->GetLocalMatrix().TransformPoint(Vector3::Zero()),
			Vector3(3.0f, 4.0f, 0.0f)),
			"RectTransform local matrix includes its calculated layout position");
	}

	void TestRectSizeDoesNotScaleChildren()
	{
		SceneBase scene;
		Actor* parent = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "Panel")));
		Actor* child = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::UI,
				Actor::InitDesc(true, TAG_NONE, "Image")),
			parent->GetHandle());
		Canvas* canvas = parent->GetComponentByClass<Canvas>();
		scene.SetCanvasScaleMode(canvas, CanvasScaleMode::ConstantPixelSize);

		RectTransform* parentTransform =
			parent->GetComponentByClass<RectTransform>();
		RectTransform* childTransform =
			child->GetComponentByClass<RectTransform>();

		parentTransform->SetAnchorMode(AnchorMode::MiddleCenter);
		parentTransform->SetSizeDelta({100.0f, 100.0f});
		childTransform->SetAnchorMode(AnchorMode::MiddleCenter);
		childTransform->SetAnchoredPosition({10.0f, 0.0f});
		childTransform->SetSizeDelta({20.0f, 20.0f});

		scene.Update(0.0f);

		Check(Near(
			childTransform->GetWorldPosition(),
			Vector3(10.0f, 0.0f, 0.0f)),
			"Parent Rect size does not multiply child layout position");
		Check(Near(
			childTransform->GetWorldScale(),
			Vector3::One()),
			"Parent Rect size does not multiply child Transform scale");
	}

	void TestRectTransformInheritsRectTransformScale()
	{
		SceneBase scene;
		Actor* parent = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "ScaledPanel")));
		Actor* child = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::UI,
				Actor::InitDesc(true, TAG_NONE, "ScaledImage")),
			parent->GetHandle());
		Canvas* canvas = parent->GetComponentByClass<Canvas>();
		scene.SetCanvasScaleMode(canvas, CanvasScaleMode::ConstantPixelSize);

		RectTransform* parentTransform =
			parent->GetComponentByClass<RectTransform>();
		RectTransform* childTransform =
			child->GetComponentByClass<RectTransform>();

		parentTransform->SetAnchorMode(AnchorMode::MiddleCenter);
		parentTransform->SetLocalScale({2.0f, 3.0f, 1.0f});
		parentTransform->SetSizeDelta({100.0f, 100.0f});
		childTransform->SetAnchorMode(AnchorMode::MiddleCenter);
		childTransform->SetAnchoredPosition({10.0f, 20.0f});
		childTransform->SetLocalScale({4.0f, 5.0f, 1.0f});
		childTransform->SetSizeDelta({20.0f, 20.0f});

		scene.Update(0.0f);

		Check(Near(
			childTransform->GetWorldPosition(),
			Vector3(20.0f, 60.0f, 0.0f)),
			"RectTransform parent scale affects child layout position");
		Check(Near(
			childTransform->GetWorldScale(),
			Vector3(8.0f, 15.0f, 1.0f)),
			"RectTransform parent and child local scales are combined");
	}

	void TestRootScreenSpaceCanvasUsesViewportSize()
	{
		SceneBase scene;
		scene.SetViewportSize(800, 600);

		Actor* canvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "Canvas")));
		Actor* childActor = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::UI,
				Actor::InitDesc(true, TAG_NONE, "Child")),
			canvasActor->GetHandle());
		Canvas* canvas = canvasActor->GetComponentByClass<Canvas>();
		scene.SetCanvasScaleMode(canvas, CanvasScaleMode::ConstantPixelSize);

		RectTransform* canvasTransform =
			canvasActor->GetComponentByClass<RectTransform>();
		RectTransform* childTransform =
			childActor->GetComponentByClass<RectTransform>();

		canvasTransform->SetSizeDelta({1.0f, 1.0f});
		childTransform->SetAnchorMode(AnchorMode::TopRight);
		childTransform->SetPivot({0.5f, 0.5f});
		childTransform->SetAnchoredPosition(Vector2::Zero());
		childTransform->SetSizeDelta({100.0f, 50.0f});

		scene.Update(0.0f);

		Check(Near(
			childTransform->GetWorldPosition(),
			Vector3(400.0f, 300.0f, 0.0f)),
			"Direct child of a root ScreenSpace Canvas uses the scene viewport size");

		scene.SetViewportSize(1000, 800);
		scene.Update(0.0f);

		Check(Near(
			childTransform->GetWorldPosition(),
			Vector3(500.0f, 400.0f, 0.0f)),
			"ScreenSpace UI layout follows viewport resizing");
	}

	void TestScreenSpaceCanvasScaleFactor()
	{
		SceneBase scene;
		scene.SetViewportSize(960, 540);

		Actor* canvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "ScaledCanvas")));
		Actor* childActor = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::UI,
				Actor::InitDesc(true, TAG_NONE, "ScaledChild")),
			canvasActor->GetHandle());

		Canvas* canvas = canvasActor->GetComponentByClass<Canvas>();
		RectTransform* childTransform =
			childActor->GetComponentByClass<RectTransform>();

		Check(scene.SetCanvasReferenceSize(canvas, {1920.0f, 1080.0f}) &&
			scene.SetCanvasScaleMode(canvas, CanvasScaleMode::ScaleWithScreenSize) &&
			scene.SetCanvasMatchWidthOrHeight(canvas, 0.5f),
			"Scene accepts ScreenSpace Canvas scaling settings");

		childTransform->SetAnchorMode(AnchorMode::MiddleCenter);
		childTransform->SetAnchoredPosition({100.0f, 40.0f});
		childTransform->SetLocalScale({2.0f, 3.0f, 1.0f});
		scene.Update(0.0f);

		Check(std::abs(canvas->GetScaleFactor() - 0.5f) < 0.001f,
			"Matching aspect ratios produce a uniform Canvas scale");
		Check(Near(childTransform->GetWorldPosition(), Vector3(50.0f, 20.0f, 0.0f)) &&
			Near(childTransform->GetWorldScale(), Vector3(1.0f, 1.5f, 0.5f)),
			"Canvas scale affects layout without overwriting child local scale");

		scene.SetViewportSize(1920, 540);
		Check(scene.SetCanvasMatchWidthOrHeight(canvas, 0.0f),
			"Canvas accepts width matching");
		scene.Update(0.0f);
		Check(std::abs(canvas->GetScaleFactor() - 1.0f) < 0.001f &&
			Near(childTransform->GetWorldPosition(), Vector3(100.0f, 40.0f, 0.0f)),
			"Width matching uses the horizontal viewport ratio");

		Check(scene.SetCanvasMatchWidthOrHeight(canvas, 1.0f),
			"Canvas accepts height matching");
		scene.Update(0.0f);
		Check(std::abs(canvas->GetScaleFactor() - 0.5f) < 0.001f &&
			Near(childTransform->GetWorldPosition(), Vector3(50.0f, 20.0f, 0.0f)),
			"Height matching uses the vertical viewport ratio");
	}

	void TestNestedCanvasMapsReferenceResolutionToDisplaySize()
	{
		SceneBase scene;
		scene.SetViewportSize(960, 540);

		Actor* rootActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "RootCanvas")));
		Actor* nestedActor = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "NestedCanvas")),
			rootActor->GetHandle());
		Actor* childActor = scene.AddChildActor(
			ActorFactory::CreateActor(
				ActorType::UI,
				Actor::InitDesc(true, TAG_NONE, "NestedChild")),
			nestedActor->GetHandle());

		Canvas* rootCanvas = rootActor->GetComponentByClass<Canvas>();
		Canvas* nestedCanvas = nestedActor->GetComponentByClass<Canvas>();
		RectTransform* nestedTransform =
			nestedActor->GetComponentByClass<RectTransform>();
		RectTransform* childTransform =
			childActor->GetComponentByClass<RectTransform>();

		scene.SetCanvasReferenceSize(rootCanvas, {1920.0f, 1080.0f});
		scene.SetCanvasMatchWidthOrHeight(rootCanvas, 0.5f);
		nestedTransform->SetAnchorMode(AnchorMode::MiddleCenter);
		nestedTransform->SetSizeDelta({960.0f, 540.0f});
		scene.SetCanvasReferenceSize(nestedCanvas, {1920.0f, 1080.0f});
		scene.SetCanvasMatchWidthOrHeight(nestedCanvas, 0.5f);
		childTransform->SetAnchorMode(AnchorMode::MiddleCenter);
		childTransform->SetAnchoredPosition({100.0f, 40.0f});
		scene.Update(0.0f);

		Check(std::abs(rootCanvas->GetScaleFactor() - 0.5f) < 0.001f &&
			std::abs(nestedCanvas->GetScaleFactor() - 0.5f) < 0.001f,
			"Root and nested Canvases calculate their own display mapping factors");
		Check(Near(
			childTransform->GetWorldPosition(),
			Vector3(25.0f, 10.0f, 0.0f)) &&
			Near(
			childTransform->GetWorldScale(),
			Vector3(0.25f, 0.25f, 0.25f)),
			"Nested Canvas scale is inherited by child layout after the root scale");

		const Matrix4x4 nestedRenderMatrix = nestedTransform->GetWorldMatrix();
		const Vector3 nestedLeft =
			nestedRenderMatrix.TransformPoint({-0.5f, 0.0f, 0.0f});
		const Vector3 nestedRight =
			nestedRenderMatrix.TransformPoint({0.5f, 0.0f, 0.0f});

		Check(std::abs(nestedRight.x - nestedLeft.x - 480.0f) < 0.001f,
			"Nested Canvas keeps its RectTransform display width while only children inherit its scale factor");

		Check(scene.SetCanvasScaleMode(
			nestedCanvas,
			CanvasScaleMode::ConstantPixelSize),
			"Nested Canvas accepts ConstantPixelSize");
		scene.Update(0.0f);

		Check(std::abs(nestedCanvas->GetScaleFactor() - 1.0f) < 0.001f &&
			Near(
				childTransform->GetWorldPosition(),
				Vector3(50.0f, 20.0f, 0.0f)),
			"ConstantPixelSize uses the nested RectTransform display size as its layout space");
	}

	void TestTransformToRectTransformConversion()
	{
		Transform source;
		const Quaternion rotation =
			Quaternion::CreateFromEulerDeg({10.0f, 20.0f, 30.0f});
		source.SetParams({
			.localPosition = {12.0f, -4.0f, 7.0f},
			.localRotation = rotation,
			.localScale = {2.0f, 3.0f, 4.0f},
			.name = "ConvertedTransform"
		});

		nlohmann::json before;
		source.Serialize(before);

		const TransformSnapshot snapshot =
			TransformConversion::Capture(source);
		std::unique_ptr<Transform> converted =
			TransformConversion::Create(
				TransformKind::RectTransform,
				snapshot);
		RectTransform* rectTransform =
			dynamic_cast<RectTransform*>(converted.get());

		Check(snapshot.sourceKind == TransformKind::Transform,
			"Transform conversion snapshot records its concrete source kind");
		Check(rectTransform != nullptr,
			"Transform conversion creates a RectTransform");
		Check(rectTransform &&
			Near(rectTransform->GetLocalPosition(), Vector3::Zero()) &&
			Near(rectTransform->GetAnchoredPosition(), {12.0f, -4.0f}) &&
			rectTransform->GetLocalRotationQuat().NearEqual(rotation, 0.001f) &&
			Near(rectTransform->GetLocalScale(), {2.0f, 3.0f, 4.0f}),
			"Transform-to-RectTransform conversion preserves common pose data");
		Check(rectTransform &&
			rectTransform->GetAnchorMode() == AnchorMode::MiddleCenter &&
			Near(rectTransform->GetPivot(), {0.5f, 0.5f}) &&
			Near(rectTransform->GetSize(), {100.0f, 100.0f}),
			"Transform-to-RectTransform conversion applies safe layout defaults");

		nlohmann::json after;
		source.Serialize(after);
		Check(after == before,
			"Transform conversion does not mutate its source component");
	}

	void TestRectTransformDeserializeClearsHiddenPosition()
	{
		RectTransform source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		serialized["position"] = {12.0f, -4.0f, 9.0f};

		RectTransform restored;
		Check(restored.Deserialize(serialized),
			"RectTransform accepts a valid legacy Transform position");
		Check(Near(restored.GetLocalPosition(), Vector3::Zero()),
			"RectTransform discards hidden XYZ position during deserialization");
	}

	void TestRectTransformToTransformConversion()
	{
		SceneBase scene;
		Actor* actor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "RectSource")));
		RectTransform* source =
			actor->GetComponentByClass<RectTransform>();
		const Quaternion rotation =
			Quaternion::CreateFromEulerDeg({0.0f, 0.0f, 15.0f});

		static_cast<Transform&>(*source).SetParams({
			.localPosition = {0.0f, 0.0f, 3.0f},
			.localRotation = rotation,
			.localScale = {1.5f, 2.0f, 1.0f},
			.name = "ConvertedRectTransform"
		});
		source->SetParams({
			.anchorMode = AnchorMode::TopLeft,
			.anchoredPosition = {10.0f, -20.0f},
			.pivot = {0.25f, 0.75f},
			.size = {200.0f, 100.0f},
			.name = "ConvertedRectTransform"
		});

		scene.Update(0.0f);

		Transform3D expectedLocal;
		source->GetLocalMatrix().Decompose(
			expectedLocal.position,
			expectedLocal.rotation,
			expectedLocal.scale);

		const TransformSnapshot snapshot =
			TransformConversion::Capture(*source);
		std::unique_ptr<Transform> converted =
			TransformConversion::Create(
				TransformKind::Transform,
				snapshot);

		Check(snapshot.sourceKind == TransformKind::RectTransform,
			"RectTransform snapshot records its concrete source kind");
		Check(converted &&
			dynamic_cast<RectTransform*>(converted.get()) == nullptr,
			"RectTransform conversion creates a normal Transform");
		Check(converted &&
			Near(converted->GetLocalPosition(), expectedLocal.position) &&
			converted->GetLocalRotationQuat().NearEqual(
				expectedLocal.rotation, 0.001f) &&
			Near(converted->GetLocalScale(), expectedLocal.scale),
			"RectTransform-to-Transform conversion preserves effective local layout");
	}

	void TestRectTransformStateReconstruction()
	{
		RectTransform source;
		static_cast<Transform&>(source).SetParams({
			.localPosition = {0.0f, 0.0f, 5.0f},
			.localRotation =
				Quaternion::CreateFromEulerDeg({1.0f, 2.0f, 3.0f}),
			.localScale = {2.0f, 2.0f, 1.0f},
			.name = "ReconstructedRect"
		});
		source.SetParams({
			.anchorMode = AnchorMode::BottomRight,
			.anchoredPosition = {-30.0f, 40.0f},
			.pivot = {0.2f, 0.8f},
			.size = {320.0f, 180.0f},
			.name = "ReconstructedRect"
		});

		const TransformSnapshot snapshot =
			TransformConversion::Capture(source);
		std::unique_ptr<Transform> converted =
			TransformConversion::Create(
				TransformKind::RectTransform,
				snapshot);
		RectTransform* restored =
			dynamic_cast<RectTransform*>(converted.get());

		Check(restored &&
			Near(restored->GetLocalTransform().position, Vector3::Zero()) &&
			restored->GetLocalRotationQuat().NearEqual(
				source.GetLocalRotationQuat(), 0.001f) &&
			Near(restored->GetLocalScale(), source.GetLocalScale()),
			"RectTransform reconstruction canonicalizes position and preserves rotation and scale");
		Check(restored &&
			restored->GetAnchorMode() == source.GetAnchorMode() &&
			Near(restored->GetAnchoredPosition(),
				source.GetAnchoredPosition()) &&
			Near(restored->GetPivot(), source.GetPivot()) &&
			Near(restored->GetSize(), source.GetSize()),
			"RectTransform reconstruction preserves all layout state");
	}

	void TestInvalidTransformConversionKind()
	{
		Transform source;
		const TransformSnapshot snapshot =
			TransformConversion::Capture(source);

		Check(
			TransformConversion::Create(
				static_cast<TransformKind>(999),
				snapshot) == nullptr,
			"Transform conversion safely rejects an invalid target kind");
	}

	void TestRendererComponentRoundTrip()
	{
		TestRendererComponent source;
		source.SetName("SavedRenderer");
		source.SetColor({2.5f, 0.25f, 4.0f, 0.75f});
		source.SetVisible(false);

		nlohmann::json serialized;
		Check(source.Serialize(serialized),
			"RendererComponent Serialize succeeds");

		TestRendererComponent restored;
		Check(restored.Deserialize(serialized),
			"RendererComponent Deserialize succeeds");
		Check(restored.GetName() == "SavedRenderer" &&
			Near(restored.GetColor(), {2.5f, 0.25f, 4.0f, 0.75f}) &&
			!restored.IsVisible(),
			"RendererComponent settings and HDR color survive a round trip");
	}

	void TestInvalidRendererJsonDoesNotPartiallyMutate()
	{
		TestRendererComponent renderer;
		renderer.SetName("OriginalRenderer");
		renderer.SetColor({0.1f, 0.2f, 0.3f, 0.4f});
		renderer.SetVisible(false);

		nlohmann::json before;
		renderer.Serialize(before);
		nlohmann::json invalid = before;
		invalid["name"] = "MutatedRenderer";
		invalid["visible"] = true;
		invalid["color"] = "invalid";

		Check(!renderer.Deserialize(invalid),
			"RendererComponent rejects invalid JSON");
		nlohmann::json after;
		renderer.Serialize(after);
		Check(after == before,
			"Rejected JSON leaves the RendererComponent unchanged");
	}

	void TestRendererRejectsNonFiniteColor()
	{
		TestRendererComponent source;
		nlohmann::json invalid;
		source.Serialize(invalid);
		invalid["color"][0] = std::numeric_limits<float>::infinity();

		TestRendererComponent restored;
		Check(!restored.Deserialize(invalid),
			"RendererComponent rejects a non-finite color");
	}

	void TestDefaultRendererComponentRoundTrip()
	{
		TestRendererComponent source;
		nlohmann::json serialized;
		source.Serialize(serialized);

		TestRendererComponent restored;
		Check(restored.Deserialize(serialized),
			"Default RendererComponent survives its own serialized schema");
	}

	void TestMeshRendererPendingAssetRoundTrip()
	{
		const Guid assetId = GuidGenerator::Generate();
		MeshRenderer source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		serialized["name"] = "SavedMeshRenderer";
		serialized["color"] = {1.5f, 0.25f, 0.5f, 0.75f};
		serialized["visible"] = false;
		serialized["meshAssetId"] = assetId.ToString();

		MeshRenderer restored;
		Check(restored.Deserialize(serialized),
			"MeshRenderer accepts a valid deferred asset Guid");
		Check(restored.GetAssetId() == assetId && !restored.IsConfigured(),
			"MeshRenderer exposes its pending asset without creating runtime resources");

		nlohmann::json restoredJson;
		restored.Serialize(restoredJson);
		Check(restoredJson["meshAssetId"] == assetId.ToString() &&
			restoredJson["name"] == "SavedMeshRenderer" &&
			!restoredJson["visible"].get<bool>() &&
			Near(restored.GetColor(), {1.5f, 0.25f, 0.5f, 0.75f}),
			"MeshRenderer preserves pending asset and common renderer settings");
	}

	void TestInvalidMeshRendererGuidDoesNotPartiallyMutate()
	{
		MeshRenderer renderer;
		SubmeshRenderTemplate runtimeTemplate;
		renderer.SetParams({
			.templates = {runtimeTemplate},
			.color = {0.1f, 0.2f, 0.3f, 0.4f},
			.visible = false,
			.name = "OriginalMeshRenderer"
		});

		nlohmann::json invalid;
		renderer.Serialize(invalid);
		invalid["name"] = "MutatedMeshRenderer";
		invalid["color"] = {9.0f, 9.0f, 9.0f, 9.0f};
		invalid["visible"] = true;
		invalid["meshAssetId"] = "not-a-guid";

		Check(!renderer.Deserialize(invalid),
			"MeshRenderer rejects an invalid asset Guid");
		Check(renderer.GetName() == "OriginalMeshRenderer" &&
			Near(renderer.GetColor(), {0.1f, 0.2f, 0.3f, 0.4f}) &&
			!renderer.IsVisible() && renderer.IsConfigured(),
			"Invalid asset Guid leaves MeshRenderer unchanged");
	}

	void TestMeshRendererResolveWithoutContextFailsSafely()
	{
		const Guid assetId = GuidGenerator::Generate();
		SceneBase scene;
		auto actorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "MeshOwner"));
		MeshRenderer* renderer = actorOwned->AddComponent<MeshRenderer>();
		scene.AddRootActor(std::move(actorOwned));

		nlohmann::json serialized;
		renderer->Serialize(serialized);
		serialized["meshAssetId"] = assetId.ToString();
		Check(renderer->Deserialize(serialized),
			"Owned MeshRenderer stores a pending asset Guid");
		Check(!renderer->ResolveReferences(scene),
			"MeshRenderer reports missing EngineContext during resolution");

		nlohmann::json unresolvedJson;
		renderer->Serialize(unresolvedJson);
		Check(unresolvedJson["meshAssetId"] == assetId.ToString(),
			"Failed MeshRenderer resolution retains the pending asset Guid");
	}

	void TestMeshRendererMissingAssetIsRecoverable()
	{
		const Guid missingAssetId = GuidGenerator::Generate();

		AssetManager assetManager;
		MeshManager meshManager;
		EngineContext context{
			.pMeshManager = &meshManager,
			.pAssetManager = &assetManager
		};

		SceneBase scene;
		scene.Initialize(context);

		auto actorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "MissingMeshOwner"));
		MeshRenderer* renderer = actorOwned->AddComponent<MeshRenderer>();
		scene.AddRootActor(std::move(actorOwned));

		nlohmann::json serialized;
		renderer->Serialize(serialized);
		serialized["meshAssetId"] = missingAssetId.ToString();

		Check(renderer->Deserialize(serialized),
			"MeshRenderer accepts a missing asset Guid as a deferred reference");
		Check(renderer->ResolveReferences(scene),
			"Missing mesh asset does not fail scene reference restoration");
		Check(renderer->GetAssetId() == missingAssetId &&
			!renderer->IsConfigured(),
			"Missing mesh asset remains visible to the inspector without runtime templates");

		nlohmann::json unresolvedJson;
		renderer->Serialize(unresolvedJson);
		Check(unresolvedJson["meshAssetId"] == missingAssetId.ToString(),
			"Recoverable missing mesh reference survives serialization");
	}

	void TestMeshRendererSetParamsClearsAssetAssociation()
	{
		const Guid assetId = GuidGenerator::Generate();
		MeshRenderer renderer;
		nlohmann::json serialized;
		renderer.Serialize(serialized);
		serialized["meshAssetId"] = assetId.ToString();
		renderer.Deserialize(serialized);

		SubmeshRenderTemplate runtimeTemplate;
		renderer.SetParams({.templates = {runtimeTemplate}});

		nlohmann::json afterSetParams;
		renderer.Serialize(afterSetParams);
		Check(afterSetParams["meshAssetId"].is_null() &&
			renderer.IsConfigured(),
			"Direct MeshRenderer templates clear the old asset association");
	}

	void TestDefaultMeshRendererRoundTrip()
	{
		MeshRenderer source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		Check(serialized["meshAssetId"].is_null(),
			"Default MeshRenderer serializes a null meshAssetId");

		MeshRenderer restored;
		Check(restored.Deserialize(serialized),
			"Default MeshRenderer survives its own serialized schema");
	}

	void TestRendererCanvasSortOrderRoundTrip()
	{
		MeshRenderer source;
		source.SetSortOrderInCanvas(37);

		nlohmann::json serialized;
		source.Serialize(serialized);

		MeshRenderer restored;
		Check(restored.Deserialize(serialized) &&
			restored.GetSortOrderInCanvas() == 37,
			"Renderer Canvas sort order survives serialization");

		serialized.erase("sortOrderInCanvas");
		MeshRenderer legacy;
		legacy.SetSortOrderInCanvas(99);
		Check(legacy.Deserialize(serialized) &&
			legacy.GetSortOrderInCanvas() == 0,
			"Renderer without a saved Canvas sort order uses the compatibility default");
	}

	void TestMeshRendererFitsRectWithoutDistortingDepth()
	{
		SceneBase scene;
		Actor* canvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "MeshFitCanvas")));
		Canvas* canvas = canvasActor->GetComponentByClass<Canvas>();
		scene.SetCanvasScaleMode(canvas, CanvasScaleMode::ConstantPixelSize);

		Actor* meshActor = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "MeshFitActor")),
			canvasActor->GetHandle());
		MeshRenderer* renderer = meshActor->AddComponent<MeshRenderer>();
		RectTransform* rectTransform =
			meshActor->GetComponentByClass<RectTransform>();

		SubmeshRenderTemplate renderTemplate;
		renderTemplate.meshDesc.boundsCenter = {1.0f, 0.0f, 0.0f};
		renderTemplate.meshDesc.boundsRadius = 1.0f;
		renderer->SetParams({.templates = {renderTemplate}});
		rectTransform->SetSizeDelta({200.0f, 100.0f});
		scene.Update(0.0f);

		Transform3D fittedTransform;
		renderer->GetRenderProxy().common.worldMatrix.Decompose(
			fittedTransform.position,
			fittedTransform.rotation,
			fittedTransform.scale);

		Check(Near(fittedTransform.scale, Vector3(25.0f, 25.0f, 25.0f)),
			"MeshRenderer fits Rect size with one uniform XYZ scale");

		renderTemplate.meshDesc.boundsCenter = Vector3::Zero();
		renderTemplate.meshDesc.boundsRadius = 0.0f;
		renderer->SetParams({.templates = {renderTemplate}});

		Transform3D fallbackTransform;
		renderer->GetRenderProxy().common.worldMatrix.Decompose(
			fallbackTransform.position,
			fallbackTransform.rotation,
			fallbackTransform.scale);

		Check(Near(fallbackTransform.scale, Vector3::One()),
			"MeshRenderer with missing Bounds preserves its Transform scale");
	}

	void TestSpriteRendererPendingAssetRoundTrip()
	{
		const Guid textureAssetId = GuidGenerator::Generate();
		SpriteRenderer source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		serialized["name"] = "SavedSpriteRenderer";
		serialized["color"] = {1.5f, 0.25f, 0.5f, 0.75f};
		serialized["visible"] = false;
		serialized["textureAssetId"] = textureAssetId.ToString();
		serialized["uvScale"] = {2.0f, 3.0f};
		serialized["uvOffset"] = {0.1f, 0.2f};
		serialized["pivot"] = {0.25f, 0.75f};
		serialized["billboardType"] = static_cast<int>(BillboardType::Cylindrical);
		serialized["flipX"] = true;
		serialized["flipY"] = false;

		SpriteRenderer restored;
		Check(restored.Deserialize(serialized),
			"SpriteRenderer accepts a valid deferred texture Guid");
		Check(restored.GetTextureAssetId() == textureAssetId && !restored.IsConfigured(),
			"SpriteRenderer exposes its pending texture without creating runtime resources");

		nlohmann::json restoredJson;
		restored.Serialize(restoredJson);
		Check(restoredJson["textureAssetId"] == textureAssetId.ToString() &&
			restored.GetName() == "SavedSpriteRenderer" &&
			Near(restored.GetColor(), {1.5f, 0.25f, 0.5f, 0.75f}) &&
			!restored.IsVisible() &&
			Near(restored.GetUVScale(), {2.0f, 3.0f}) &&
			Near(restored.GetUVOffset(), {0.1f, 0.2f}) &&
			Near(restored.GetPivot(), {0.25f, 0.75f}) &&
			restored.GetBillboardType() == BillboardType::Cylindrical &&
			restored.IsFlipX() && !restored.IsFlipY(),
			"SpriteRenderer preserves pending texture and all component settings");
	}

	void TestInvalidSpriteRendererGuidDoesNotPartiallyMutate()
	{
		SpriteRenderer renderer;
		SpriteRenderTemplate runtimeTemplate;
		runtimeTemplate.materialDesc.textureHandle = 1;
		renderer.SetParams({
			.renderTemplate = runtimeTemplate,
			.uvScale = {2.0f, 2.0f},
			.uvOffset = {0.25f, 0.5f},
			.pivot = {0.4f, 0.6f},
			.billboardType = BillboardType::Spherical,
			.flipX = true,
			.flipY = true,
			.color = {0.1f, 0.2f, 0.3f, 0.4f},
			.visible = false,
			.name = "OriginalSpriteRenderer"
		});

		nlohmann::json invalid;
		renderer.Serialize(invalid);
		invalid["name"] = "MutatedSpriteRenderer";
		invalid["uvScale"] = {9.0f, 9.0f};
		invalid["textureAssetId"] = "not-a-guid";

		Check(!renderer.Deserialize(invalid),
			"SpriteRenderer rejects an invalid texture Guid");
		Check(renderer.GetName() == "OriginalSpriteRenderer" &&
			Near(renderer.GetUVScale(), {2.0f, 2.0f}) &&
			renderer.IsConfigured(),
			"Invalid texture Guid leaves SpriteRenderer unchanged");
	}

	void TestSpriteRendererValidationBoundaries()
	{
		SpriteRenderer source;
		nlohmann::json valid;
		source.Serialize(valid);

		SpriteRenderer restored;
		nlohmann::json invalidBillboard = valid;
		invalidBillboard["billboardType"] = 999;
		Check(!restored.Deserialize(invalidBillboard),
			"SpriteRenderer rejects an invalid BillboardType");

		nlohmann::json invalidPivot = valid;
		invalidPivot["pivot"] = {-0.1f, 0.5f};
		Check(!restored.Deserialize(invalidPivot),
			"SpriteRenderer rejects an out-of-range pivot");
	}

	void TestSpriteRendererResolveWithoutContextFailsSafely()
	{
		const Guid textureAssetId = GuidGenerator::Generate();
		SceneBase scene;
		auto actorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "SpriteOwner"));
		SpriteRenderer* renderer = actorOwned->AddComponent<SpriteRenderer>();
		scene.AddRootActor(std::move(actorOwned));

		nlohmann::json serialized;
		renderer->Serialize(serialized);
		serialized["textureAssetId"] = textureAssetId.ToString();
		Check(renderer->Deserialize(serialized),
			"Owned SpriteRenderer stores a pending texture Guid");
		Check(!renderer->ResolveReferences(scene),
			"SpriteRenderer reports missing EngineContext during resolution");

		nlohmann::json unresolvedJson;
		renderer->Serialize(unresolvedJson);
		Check(unresolvedJson["textureAssetId"] == textureAssetId.ToString(),
			"Failed SpriteRenderer resolution retains the pending texture Guid");
	}

	void TestSpriteRendererMissingTextureIsRecoverable()
	{
		const Guid missingTextureId = GuidGenerator::Generate();
		AssetManager assetManager;
		TextureManager textureManager;
		EngineContext context{
			.pTextureManager = &textureManager,
			.pAssetManager = &assetManager
		};

		SceneBase scene;
		scene.Initialize(context);

		auto actorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "MissingSpriteOwner"));
		SpriteRenderer* renderer = actorOwned->AddComponent<SpriteRenderer>();
		scene.AddRootActor(std::move(actorOwned));

		nlohmann::json serialized;
		renderer->Serialize(serialized);
		serialized["textureAssetId"] = missingTextureId.ToString();

		Check(renderer->Deserialize(serialized) &&
			renderer->ResolveReferences(scene),
			"Missing sprite texture does not fail scene reference restoration");
		Check(renderer->GetTextureAssetId() == missingTextureId &&
			!renderer->IsConfigured(),
			"Missing sprite texture remains visible without runtime resources");
	}

	void TestSpriteRendererSetParamsClearsAssetAssociation()
	{
		const Guid textureAssetId = GuidGenerator::Generate();
		SpriteRenderer renderer;
		nlohmann::json serialized;
		renderer.Serialize(serialized);
		serialized["textureAssetId"] = textureAssetId.ToString();
		renderer.Deserialize(serialized);

		SpriteRenderTemplate runtimeTemplate;
		runtimeTemplate.materialDesc.textureHandle = 1;
		renderer.SetParams({.renderTemplate = runtimeTemplate});

		nlohmann::json afterSetParams;
		renderer.Serialize(afterSetParams);
		Check(afterSetParams["textureAssetId"].is_null() && renderer.IsConfigured(),
			"Direct SpriteRenderer template clears the old texture association");
	}

	void TestDefaultSpriteRendererRoundTrip()
	{
		SpriteRenderer source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		Check(serialized["textureAssetId"].is_null(),
			"Default SpriteRenderer serializes a null textureAssetId");

		SpriteRenderer restored;
		Check(restored.Deserialize(serialized),
			"Default SpriteRenderer survives its own serialized schema");
	}

	void TestCanvasRoundTrip()
	{
		Canvas source;
		source.SetParams({
			.renderMode = CanvasRenderMode::WorldSpace,
			.scaleMode = CanvasScaleMode::ConstantPixelSize,
			.sortOrder = 42,
			.isVisible = false,
			.referenceSize = {1280.0f, 720.0f},
			.matchWidthOrHeight = 0.25f,
			.name = "SavedCanvas"
		});

		nlohmann::json serialized;
		Check(source.Serialize(serialized), "Canvas Serialize succeeds");
		Check(serialized.size() == 7 &&
			serialized.contains("name") &&
			serialized.contains("renderMode") &&
			serialized.contains("scaleMode") &&
			serialized.contains("sortOrder") &&
			serialized.contains("visible") &&
			serialized.contains("referenceSize") &&
			serialized.contains("matchWidthOrHeight"),
			"Canvas Serialize excludes its runtime UI registration list");

		Canvas restored;
		Check(restored.Deserialize(serialized), "Canvas Deserialize succeeds");
		Check(restored.GetName() == "SavedCanvas" &&
			restored.GetRenderMode() == CanvasRenderMode::WorldSpace &&
			restored.GetScaleMode() == CanvasScaleMode::ConstantPixelSize &&
			restored.GetSortOrder() == 42 &&
			!restored.IsVisible() &&
			Near(restored.GetReferenceSize(), {1280.0f, 720.0f}) &&
			std::abs(restored.GetMatchWidthOrHeight() - 0.25f) < 0.001f,
			"Canvas settings survive a round trip");
	}

	void TestCanvasLoadsSchemaWithoutScalingFields()
	{
		Canvas source;
		nlohmann::json legacy;
		source.Serialize(legacy);
		legacy.erase("scaleMode");
		legacy.erase("matchWidthOrHeight");

		Canvas restored;
		Check(restored.Deserialize(legacy),
			"Canvas loads scenes saved before scaling settings were added");
		Check(restored.GetScaleMode() == CanvasScaleMode::ScaleWithScreenSize &&
			std::abs(restored.GetMatchWidthOrHeight() - 0.5f) < 0.001f,
			"Missing scaling settings use the current Canvas defaults");
	}

	void TestInvalidCanvasJsonDoesNotPartiallyMutate()
	{
		Canvas canvas;
		canvas.SetParams({
			.renderMode = CanvasRenderMode::WorldSpace,
			.sortOrder = 7,
			.isVisible = false,
			.name = "OriginalCanvas"
		});

		nlohmann::json before;
		canvas.Serialize(before);
		nlohmann::json invalid = before;
		invalid["name"] = "MutatedCanvas";
		invalid["visible"] = true;
		invalid["sortOrder"] = -1;

		Check(!canvas.Deserialize(invalid), "Canvas rejects a negative sortOrder");
		nlohmann::json after;
		canvas.Serialize(after);
		Check(after == before, "Rejected JSON leaves the Canvas unchanged");
	}

	void TestCanvasRenderModeValidation()
	{
		Canvas source;
		source.SetParams({
			.renderMode = CanvasRenderMode::WorldSpace,
			.sortOrder = 3,
			.isVisible = false,
			.name = "OriginalCanvas"
		});

		nlohmann::json before;
		source.Serialize(before);

		nlohmann::json negative = before;
		negative["renderMode"] = -1;
		Check(!source.Deserialize(negative),
			"Canvas rejects a negative renderMode");

		nlohmann::json sentinel = before;
		sentinel["renderMode"] =
			static_cast<int>(CanvasRenderMode::Max);
		Check(!source.Deserialize(sentinel),
			"Canvas rejects the CanvasRenderMode sentinel");

		nlohmann::json after;
		source.Serialize(after);
		Check(after == before,
			"Rejected renderMode values leave the Canvas unchanged");
	}

	void TestCanvasReferenceSizeValidation()
	{
		Canvas source;
		nlohmann::json before;
		source.Serialize(before);

		const auto rejectsWithoutMutation =
			[&](const Vector2& invalidSize, const char* message)
			{
				nlohmann::json invalid = before;
				invalid["referenceSize"] =
					JsonMath::ToJson(invalidSize);

				Check(!source.Deserialize(invalid), message);

				nlohmann::json after;
				source.Serialize(after);
				Check(after == before,
					"Rejected referenceSize leaves the Canvas unchanged");
			};

		rejectsWithoutMutation(
			{0.0f, 1080.0f},
			"Canvas rejects a zero referenceSize axis");
		rejectsWithoutMutation(
			{-1.0f, 1080.0f},
			"Canvas rejects a negative referenceSize axis");
		rejectsWithoutMutation(
			{(std::numeric_limits<float>::quiet_NaN)(), 1080.0f},
			"Canvas rejects a NaN referenceSize axis");
		rejectsWithoutMutation(
			{(std::numeric_limits<float>::infinity)(), 1080.0f},
			"Canvas rejects an infinite referenceSize axis");

		SceneBase scene;
		Actor* canvasActor = scene.AddRootActor(
			ActorFactory::CreateActor(
				ActorType::Canvas,
				Actor::InitDesc(true, TAG_NONE, "Canvas")));
		Canvas* canvas = canvasActor->GetComponentByClass<Canvas>();

		Check(!scene.SetCanvasReferenceSize(
			canvas,
			{(std::numeric_limits<float>::quiet_NaN)(), 1080.0f}),
			"Scene rejects a non-finite Canvas reference size");
		Check(scene.SetCanvasReferenceSize(
			canvas,
			{640.0f, 360.0f}) &&
			Near(canvas->GetReferenceSize(), {640.0f, 360.0f}),
			"Scene applies a valid Canvas reference size");
	}

	void TestCanvasSortOrderUpperBoundary()
	{
		Canvas source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		serialized["sortOrder"] =
			static_cast<uint64_t>((std::numeric_limits<UINT>::max)()) + 1ULL;

		Canvas restored;
		Check(!restored.Deserialize(serialized),
			"Canvas rejects a sortOrder larger than UINT_MAX");
	}

	void TestDefaultCanvasRoundTrip()
	{
		Canvas source;
		nlohmann::json serialized;
		source.Serialize(serialized);

		Canvas restored;
		Check(restored.Deserialize(serialized),
			"Default Canvas survives its own serialized schema");
		Check(restored.GetRenderMode() == CanvasRenderMode::ScreenSpace,
			"Default Canvas uses ScreenSpace rendering");
	}

	void TestUIRendererRoundTripAndCanvasResolution()
	{
		SceneBase scene;
		auto canvasActorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "CanvasActor"));
		Canvas* canvas = canvasActorOwned->AddComponent<Canvas>();
		Actor* canvasActor = scene.AddRootActor(std::move(canvasActorOwned));

		auto uiActorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "UIActor"));
		UIRenderer* renderer = uiActorOwned->AddComponent<UIRenderer>();
		scene.AddRootActor(std::move(uiActorOwned));

		nlohmann::json serialized;
		renderer->Serialize(serialized);
		serialized["name"] = "SavedUIRenderer";
		serialized["color"] = {0.5f, 1.5f, 0.25f, 0.75f};
		serialized["visible"] = false;
		serialized["canvasActorId"] = canvasActor->GetGuid().ToString();
		serialized["order"] = 12;
		serialized["uvScale"] = {2.0f, 3.0f};
		serialized["uvOffset"] = {0.1f, 0.2f};
		serialized["flipX"] = true;
		serialized["flipY"] = false;

		Check(renderer->Deserialize(serialized),
			"UIRenderer with a Canvas Guid deserializes");
		Check(renderer->GetCanvas() == nullptr,
			"UIRenderer defers Canvas binding during Deserialize");
		Check(renderer->ResolveReferences(scene),
			"UIRenderer resolves its Canvas Actor Guid");
		Check(renderer->GetCanvas() == canvas &&
			renderer->GetName() == "SavedUIRenderer" &&
			renderer->GetOrder() == 12 &&
			Near(renderer->GetUVScale(), {2.0f, 3.0f}) &&
			Near(renderer->GetUVOffset(), {0.1f, 0.2f}) &&
			renderer->IsFlipX() && !renderer->IsFlipY(),
			"UIRenderer settings and Canvas survive resolution");

		nlohmann::json resolvedJson;
		Check(renderer->Serialize(resolvedJson) &&
			resolvedJson["canvasActorId"] == canvasActor->GetGuid().ToString(),
			"Resolved UIRenderer serializes its Canvas Actor Guid");
	}

	void TestUIRendererMissingCanvasReferenceFailsSafely()
	{
		const Guid missingGuid = GuidGenerator::Generate();
		UIRenderer renderer;
		nlohmann::json serialized;
		renderer.Serialize(serialized);
		serialized["canvasActorId"] = missingGuid.ToString();
		Check(renderer.Deserialize(serialized),
			"UIRenderer accepts a valid unresolved Canvas Guid");

		SceneBase scene;
		Check(!renderer.ResolveReferences(scene),
			"UIRenderer reports a missing Canvas Actor");
		nlohmann::json unresolvedJson;
		renderer.Serialize(unresolvedJson);
		Check(unresolvedJson["canvasActorId"] == missingGuid.ToString(),
			"Failed Canvas resolution retains the pending Guid");
	}

	void TestUIRendererRejectsActorWithoutCanvas()
	{
		SceneBase scene;
		Actor* nonCanvasActor = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "NotCanvas")));

		UIRenderer renderer;
		nlohmann::json serialized;
		renderer.Serialize(serialized);
		serialized["canvasActorId"] = nonCanvasActor->GetGuid().ToString();
		renderer.Deserialize(serialized);

		Check(!renderer.ResolveReferences(scene),
			"UIRenderer rejects an Actor without a Canvas component");
	}

	void TestInvalidUIRendererJsonDoesNotPartiallyMutate()
	{
		UIRenderer renderer;
		renderer.SetName("OriginalUIRenderer");
		renderer.SetColor({0.1f, 0.2f, 0.3f, 0.4f});
		renderer.SetVisible(false);
		renderer.SetOrder(5);
		renderer.SetUVScale({2.0f, 2.0f});

		nlohmann::json before;
		renderer.Serialize(before);
		nlohmann::json invalid = before;
		invalid["name"] = "MutatedUIRenderer";
		invalid["visible"] = true;
		invalid["order"] = -1;

		Check(!renderer.Deserialize(invalid),
			"UIRenderer rejects invalid JSON");
		nlohmann::json after;
		renderer.Serialize(after);
		Check(after == before,
			"Rejected JSON leaves the UIRenderer unchanged");
	}

	void TestDefaultUIRendererRoundTrip()
	{
		UIRenderer source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		Check(serialized["canvasActorId"].is_null(),
			"Default UIRenderer serializes a null Canvas reference");

		UIRenderer restored;
		Check(restored.Deserialize(serialized),
			"Default UIRenderer survives its own serialized schema");
	}

	void TestUIImagePendingTextureRoundTrip()
	{
		const Guid textureAssetId = GuidGenerator::Generate();
		UIImage source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		serialized["name"] = "SavedUIImage";
		serialized["color"] = {0.25f, 0.5f, 1.5f, 0.75f};
		serialized["visible"] = false;
		serialized["order"] = 9;
		serialized["uvScale"] = {2.0f, 3.0f};
		serialized["uvOffset"] = {0.1f, 0.2f};
		serialized["flipX"] = true;
		serialized["flipY"] = false;
		serialized["textureAssetId"] = textureAssetId.ToString();

		UIImage restored;
		Check(restored.Deserialize(serialized),
			"UIImage accepts a valid deferred texture Guid");
		Check(restored.GetTextureAssetId() == textureAssetId && !restored.IsConfigured(),
			"UIImage exposes its pending texture without creating runtime resources");

		nlohmann::json restoredJson;
		restored.Serialize(restoredJson);
		Check(restoredJson["textureAssetId"] == textureAssetId.ToString() &&
			restored.GetName() == "SavedUIImage" &&
			restored.GetOrder() == 9 &&
			Near(restored.GetColor(), {0.25f, 0.5f, 1.5f, 0.75f}) &&
			Near(restored.GetUVScale(), {2.0f, 3.0f}) &&
			Near(restored.GetUVOffset(), {0.1f, 0.2f}) &&
			restored.IsFlipX() && !restored.IsFlipY(),
			"UIImage preserves pending texture and inherited UI settings");
	}

	void TestInvalidUIImageGuidDoesNotPartiallyMutate()
	{
		UIImage image;
		UIRenderElement element;
		element.materialDesc.textureHandle = 1;
		image.SetParams({
			.renderTemplate = {element},
			.order = 3,
			.color = {0.1f, 0.2f, 0.3f, 0.4f},
			.uvScale = {2.0f, 2.0f},
			.uvOffset = {0.25f, 0.5f},
			.flipX = true,
			.flipY = true,
			.isVisible = false,
			.name = "OriginalUIImage"
		});

		nlohmann::json invalid;
		image.Serialize(invalid);
		invalid["name"] = "MutatedUIImage";
		invalid["order"] = 99;
		invalid["textureAssetId"] = "not-a-guid";

		Check(!image.Deserialize(invalid),
			"UIImage rejects an invalid texture Guid");
		Check(image.GetName() == "OriginalUIImage" &&
			image.GetOrder() == 3 && image.IsConfigured(),
			"Invalid texture Guid leaves UIImage unchanged");
	}

	void TestUIImageTextureFailureDoesNotApplyCanvas()
	{
		SceneBase scene;
		auto canvasActorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "CanvasActor"));
		canvasActorOwned->AddComponent<Canvas>();
		Actor* canvasActor = scene.AddRootActor(std::move(canvasActorOwned));

		auto imageActorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "ImageActor"));
		UIImage* image = imageActorOwned->AddComponent<UIImage>();
		scene.AddRootActor(std::move(imageActorOwned));

		const Guid textureAssetId = GuidGenerator::Generate();
		nlohmann::json serialized;
		image->Serialize(serialized);
		serialized["canvasActorId"] = canvasActor->GetGuid().ToString();
		serialized["textureAssetId"] = textureAssetId.ToString();
		Check(image->Deserialize(serialized),
			"UIImage stores pending Canvas and Texture references");
		Check(!image->ResolveReferences(scene),
			"UIImage reports missing Texture resolution context");
		Check(image->GetCanvas() == nullptr,
			"Texture failure does not partially apply the Canvas reference");

		nlohmann::json unresolvedJson;
		image->Serialize(unresolvedJson);
		Check(unresolvedJson["canvasActorId"] == canvasActor->GetGuid().ToString() &&
			unresolvedJson["textureAssetId"] == textureAssetId.ToString(),
			"Failed UIImage resolution retains both pending references");
	}

	void TestUIImageCanvasOnlyResolution()
	{
		SceneBase scene;
		auto canvasActorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "CanvasOnlyActor"));
		Canvas* canvas = canvasActorOwned->AddComponent<Canvas>();
		Actor* canvasActor = scene.AddRootActor(std::move(canvasActorOwned));

		UIImage image;
		nlohmann::json serialized;
		image.Serialize(serialized);
		serialized["canvasActorId"] = canvasActor->GetGuid().ToString();
		image.Deserialize(serialized);

		Check(image.ResolveReferences(scene) && image.GetCanvas() == canvas,
			"UIImage resolves a Canvas when no Texture is assigned");
	}

	void TestUIImageMissingTextureIsRecoverable()
	{
		const Guid missingTextureId = GuidGenerator::Generate();
		AssetManager assetManager;
		TextureManager textureManager;
		EngineContext context{
			.pTextureManager = &textureManager,
			.pAssetManager = &assetManager
		};

		SceneBase scene;
		scene.Initialize(context);

		auto actorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "MissingUIImageOwner"));
		UIImage* image = actorOwned->AddComponent<UIImage>();
		scene.AddRootActor(std::move(actorOwned));

		nlohmann::json serialized;
		image->Serialize(serialized);
		serialized["textureAssetId"] = missingTextureId.ToString();

		Check(image->Deserialize(serialized) &&
			image->ResolveReferences(scene),
			"Missing UIImage texture does not fail scene reference restoration");
		Check(image->GetTextureAssetId() == missingTextureId &&
			!image->IsConfigured(),
			"Missing UIImage texture remains visible without runtime resources");
	}

	void TestUIImageSetParamsClearsTextureAssociation()
	{
		const Guid textureAssetId = GuidGenerator::Generate();
		UIImage image;
		nlohmann::json serialized;
		image.Serialize(serialized);
		serialized["textureAssetId"] = textureAssetId.ToString();
		image.Deserialize(serialized);

		UIRenderElement element;
		element.materialDesc.textureHandle = 1;
		image.SetParams({.renderTemplate = {element}});

		nlohmann::json afterSetParams;
		image.Serialize(afterSetParams);
		Check(afterSetParams["textureAssetId"].is_null() && image.IsConfigured(),
			"Direct UIImage template clears the old Texture association");
	}

	void TestDefaultUIImageRoundTrip()
	{
		UIImage source;
		nlohmann::json serialized;
		source.Serialize(serialized);
		Check(serialized["canvasActorId"].is_null() &&
			serialized["textureAssetId"].is_null(),
			"Default UIImage serializes null Canvas and Texture references");

		UIImage restored;
		Check(restored.Deserialize(serialized),
			"Default UIImage survives its own serialized schema");
	}
}

int main()
{
	TestComponentContract();
	TestTransformRoundTrip();
	TestInvalidJsonDoesNotPartiallyMutate();
	TestSetParamsPropagatesDirtyToChildren();
	TestCameraRoundTrip();
	TestInvalidCameraJsonDoesNotPartiallyMutate();
	TestCameraReferenceResolution();
	TestCameraMissingReferenceFails();
	TestDefaultCameraRoundTrip();
	TestColliderRoundTrip();
	TestInvalidColliderJsonDoesNotPartiallyMutate();
	TestColliderValidationBoundaries();
	TestDefaultColliderRoundTrip();
	TestRectTransformRoundTrip();
	TestInvalidRectTransformUiDoesNotMutateBase();
	TestInvalidRectTransformBaseDoesNotMutateUi();
	TestRectTransformValidationBoundaries();
	TestDefaultRectTransformRoundTrip();
	TestRectTransformInheritsTransformParent();
	TestRectSizeDoesNotScaleChildren();
	TestRectTransformInheritsRectTransformScale();
	TestRootScreenSpaceCanvasUsesViewportSize();
	TestScreenSpaceCanvasScaleFactor();
	TestNestedCanvasMapsReferenceResolutionToDisplaySize();
	TestTransformToRectTransformConversion();
	TestRectTransformDeserializeClearsHiddenPosition();
	TestRectTransformToTransformConversion();
	TestRectTransformStateReconstruction();
	TestInvalidTransformConversionKind();
	TestRendererComponentRoundTrip();
	TestInvalidRendererJsonDoesNotPartiallyMutate();
	TestRendererRejectsNonFiniteColor();
	TestDefaultRendererComponentRoundTrip();
	TestMeshRendererPendingAssetRoundTrip();
	TestInvalidMeshRendererGuidDoesNotPartiallyMutate();
	TestMeshRendererResolveWithoutContextFailsSafely();
	TestMeshRendererMissingAssetIsRecoverable();
	TestMeshRendererSetParamsClearsAssetAssociation();
	TestDefaultMeshRendererRoundTrip();
	TestRendererCanvasSortOrderRoundTrip();
	TestMeshRendererFitsRectWithoutDistortingDepth();
	TestSpriteRendererPendingAssetRoundTrip();
	TestInvalidSpriteRendererGuidDoesNotPartiallyMutate();
	TestSpriteRendererValidationBoundaries();
	TestSpriteRendererResolveWithoutContextFailsSafely();
	TestSpriteRendererMissingTextureIsRecoverable();
	TestSpriteRendererSetParamsClearsAssetAssociation();
	TestDefaultSpriteRendererRoundTrip();
	TestCanvasRoundTrip();
	TestCanvasLoadsSchemaWithoutScalingFields();
	TestInvalidCanvasJsonDoesNotPartiallyMutate();
	TestCanvasRenderModeValidation();
	TestCanvasReferenceSizeValidation();
	TestCanvasSortOrderUpperBoundary();
	TestDefaultCanvasRoundTrip();
	TestUIRendererRoundTripAndCanvasResolution();
	TestUIRendererMissingCanvasReferenceFailsSafely();
	TestUIRendererRejectsActorWithoutCanvas();
	TestInvalidUIRendererJsonDoesNotPartiallyMutate();
	TestDefaultUIRendererRoundTrip();
	TestUIImagePendingTextureRoundTrip();
	TestInvalidUIImageGuidDoesNotPartiallyMutate();
	TestUIImageTextureFailureDoesNotApplyCanvas();
	TestUIImageCanvasOnlyResolution();
	TestUIImageMissingTextureIsRecoverable();
	TestUIImageSetParamsClearsTextureAssociation();
	TestDefaultUIImageRoundTrip();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " component serialization test(s) failed.\n";
		return 1;
	}

	std::cout << "All component serialization tests passed.\n";
	return 0;
}
