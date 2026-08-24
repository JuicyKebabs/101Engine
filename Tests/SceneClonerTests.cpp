#include "Scene/SceneCloner.h"

#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Context/Context.h"
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

	void TestNullSourceIsRejected()
	{
		EngineContext context;
		Check(SceneCloner::Clone(nullptr, context) == nullptr,
			"Clone rejects a null source scene");
	}

	void TestSceneStateIsClonedIndependently()
	{
		EngineContext context;
		SceneBase sourceScene;
		sourceScene.Initialize(context);

		auto cameraOwned = ActorFactory::CreateActor(
			ActorType::Camera,
			Actor::InitDesc(true, ActorTags::MainCamera, "MainCamera")
		);
		Actor* sourceCamera = cameraOwned.get();
		Check(sourceScene.AddRootActor(std::move(cameraOwned)) == sourceCamera,
			"Source main camera is registered");

		auto childOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(false, TAG_NONE, "Child")
		);
		Actor* sourceChild = childOwned.get();
		sourceChild->GetComponentByClass<Transform>()->SetLocalPosition({ 1.0f, 2.0f, 3.0f });
		Check(sourceScene.AddChildActor(std::move(childOwned), sourceCamera->GetHandle()) == sourceChild,
			"Source child actor is registered");

		const Guid cameraGuid = sourceCamera->GetGuid();
		const Guid childGuid = sourceChild->GetGuid();
		std::unique_ptr<SceneBase> clonedScene = SceneCloner::Clone(&sourceScene, context);

		Check(clonedScene != nullptr, "Scene clone succeeds");
		if (!clonedScene) return;

		Check(clonedScene.get() != &sourceScene,
			"Clone owns a distinct Scene instance");
		Check(clonedScene->GetAllActors().size() == sourceScene.GetAllActors().size(),
			"Clone preserves the actor count");

		Actor* clonedCamera = clonedScene->ResolveActor(cameraGuid);
		Actor* clonedChild = clonedScene->ResolveActor(childGuid);
		Check(clonedCamera && clonedCamera != sourceCamera,
			"Clone preserves the camera Guid without sharing the Actor");
		Check(clonedChild && clonedChild != sourceChild,
			"Clone preserves the child Guid without sharing the Actor");
		Check(clonedChild && clonedChild->GetParent() == clonedCamera,
			"Clone restores the hierarchy within the cloned Scene");
		Check(clonedChild && !clonedChild->IsActive(),
			"Clone preserves the Actor active state");

		if (clonedChild)
		{
			Transform* clonedTransform = clonedChild->GetComponentByClass<Transform>();
			const Vector3 position = clonedTransform->GetLocalPosition();
			Check(position.x == 1.0f && position.y == 2.0f && position.z == 3.0f,
				"Clone preserves component parameters");

			clonedChild->SetName("RuntimeChild");
			clonedTransform->SetLocalPosition({ 4.0f, 5.0f, 6.0f });
		}

		const Vector3 sourcePosition = sourceChild->GetComponentByClass<Transform>()->GetLocalPosition();
		Check(sourceChild->GetName() == "Child" &&
			sourcePosition.x == 1.0f && sourcePosition.y == 2.0f && sourcePosition.z == 3.0f,
			"Mutating the clone does not affect the source Scene");
		Check(clonedScene->GetCameraSystem()->GetMainCamera() ==
			(clonedCamera ? clonedCamera->GetComponentByClass<Camera>() : nullptr),
			"Clone resolves its own main camera");
	}
}

int main()
{
	TestNullSourceIsRejected();
	TestSceneStateIsClonedIndependently();

	if (g_failures == 0)
	{
		std::cout << "All SceneCloner tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " SceneCloner test(s) failed.\n";
	return 1;
}
