#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Camera.h"
#include "Engine/Scene/SceneBase.h"
#include "nlohmann/json.hpp"

#include <iostream>
#include <string>

namespace
{
	using json = nlohmann::json;

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

	Actor* AddCamera(SceneBase& scene, const char* name, const Guid* guid = nullptr)
	{
		std::unique_ptr<Actor> actor = guid
			? ActorFactory::RestoreActor(
				ActorType::Camera,
				Actor::InitDesc(true, TAG_NONE, name),
				*guid)
			: ActorFactory::CreateActor(
				ActorType::Camera,
				Actor::InitDesc(true, TAG_NONE, name));

		return scene.AddRootActor(std::move(actor));
	}

	void TestCameraReferencesSurviveTargetRecreation()
	{
		SceneBase scene;
		Actor* cameraActor = AddCamera(scene, "Camera");
		Actor* target = AddEmpty(scene, "Target");
		Camera* camera = cameraActor->GetComponentByClass<Camera>();
		const Guid targetId = target->GetGuid();

		Check(camera->SetTargetActor(target) && camera->SetFollowTarget(target),
			"Camera accepts target and follow Actor references");

		scene.RemoveActor(target);
		Check(!camera->ResolveReferences(scene),
			"Camera references reject a target pending destruction");

		json serialized;
		Check(camera->Serialize(serialized) &&
			serialized["targetActorId"] == targetId.ToString() &&
			serialized["followActorId"] == targetId.ToString(),
			"Camera preserves missing target Guids during serialization");

		scene.LateUpdate(0.0f);
		Actor* restoredTarget = scene.AddRootActor(
			ActorFactory::RestoreEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "RestoredTarget"),
				targetId));

		Check(restoredTarget && camera->ResolveReferences(scene),
			"Camera references reconnect after the target Guid is restored");

		json reserialized;
		Check(camera->Serialize(reserialized) &&
			reserialized["targetActorId"] == targetId.ToString() &&
			reserialized["followActorId"] == targetId.ToString(),
			"Reconnected Camera references retain the same persistent Guids");
	}

	void TestMainCameraSurvivesActorRecreation()
	{
		SceneBase scene;
		Actor* cameraActor = AddCamera(scene, "MainCamera");
		Camera* camera = cameraActor->GetComponentByClass<Camera>();
		const Guid cameraId = cameraActor->GetGuid();

		Check(scene.GetCameraSystem()->SetMainCamera(camera),
			"CameraSystem accepts a Camera owned by its Scene");
		Check(scene.GetCameraSystem()->GetMainCamera() == camera,
			"CameraSystem resolves the selected main Camera");

		scene.RemoveActor(cameraActor);
		Check(scene.GetCameraSystem()->GetMainCamera() == nullptr,
			"CameraSystem rejects a main Camera pending destruction");

		scene.LateUpdate(0.0f);
		Check(scene.GetCameraSystem()->GetMainCamera() == nullptr,
			"CameraSystem remains safe after main Camera collection");

		Actor* restoredActor = AddCamera(scene, "RestoredMainCamera", &cameraId);
		Camera* restoredCamera = restoredActor
			? restoredActor->GetComponentByClass<Camera>()
			: nullptr;

		Check(restoredCamera &&
			scene.GetCameraSystem()->GetMainCamera() == restoredCamera,
			"CameraSystem reconnects to a main Camera restored with the same Guid");

		scene.GetCameraSystem()->ClearMainCamera();
		Check(scene.GetCameraSystem()->GetMainCamera() == nullptr,
			"ClearMainCamera explicitly removes the persistent main Camera selection");
	}

	void TestMainCameraRejectsAnotherScene()
	{
		SceneBase scene;
		SceneBase otherScene;
		Actor* foreignCameraActor = AddCamera(otherScene, "ForeignCamera");
		Camera* foreignCamera = foreignCameraActor->GetComponentByClass<Camera>();

		Check(!scene.GetCameraSystem()->SetMainCamera(foreignCamera),
			"CameraSystem rejects a Camera owned by another Scene");
		Check(scene.GetCameraSystem()->GetMainCamera() == nullptr,
			"Rejected foreign Camera does not change the main Camera selection");
	}
}

int main()
{
	TestCameraReferencesSurviveTargetRecreation();
	TestMainCameraSurvivesActorRecreation();
	TestMainCameraRejectsAnotherScene();

	if (g_failures == 0)
	{
		std::cout << "All Camera ActorReference tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " Camera ActorReference test(s) failed.\n";
	return 1;
}
