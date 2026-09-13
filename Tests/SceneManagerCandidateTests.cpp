#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Behavior.h"
#include "Engine/Component/Camera.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneManager.h"
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace
{
	int failures = 0;

	void Check(bool condition, const char* name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { ++failures; std::cerr << "[FAIL] " << name << '\n'; }
	}

	class RedirectOnDestroy final : public Behavior
	{
	public:
		explicit RedirectOnDestroy(std::string destination)
			: m_destination(std::move(destination))
		{
		}

		static inline int destroyCount = 0;

		void Destroy() override
		{
			++destroyCount;
			ChangeScene(m_destination);
		}

	private:
		std::string m_destination;
	};

	void RegisterRedirectComponent()
	{
		TypeMetadataBuilder<RedirectOnDestroy> builder("SceneManagerRedirectOnDestroyET13");
		ComponentRegistry::Get().RegisterGameComponent(
			"SceneManagerRedirectOnDestroyET13",
			[]() -> Component* { return new RedirectOnDestroy("SceneC"); },
			typeid(RedirectOnDestroy), std::make_unique<TypeMetadata>(*builder.Build()));
	}

	struct ManagedScene
	{
		std::unique_ptr<SceneBase> scene;
		const CameraInfo* cameraInfo = nullptr;
	};

	ManagedScene MakeScene(EngineContext& context, const char* name,
		const char* redirectOnDestroy = nullptr)
	{
		ManagedScene result;
		result.scene = std::make_unique<SceneBase>();
		result.scene->Initialize(context);

		auto cameraOwned = ActorFactory::CreateActor(
			ActorType::Camera, Actor::InitDesc(true, ActorTags::MainCamera, name));
		Camera* camera = cameraOwned->GetComponentByClass<Camera>();
		result.scene->AddRootActor(std::move(cameraOwned));
		result.scene->GetCameraSystem()->SetMainCamera(camera);
		result.cameraInfo = result.scene->GetCameraSystem()->GetCameraInfo();

		if (redirectOnDestroy)
		{
			auto actor = ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "RedirectOnDestroy"));
			actor->AddComponent<RedirectOnDestroy>(redirectOnDestroy);
			result.scene->AddRootActor(std::move(actor));
		}
		return result;
	}

	void TestCandidateFailureAndTeardownReservationIsolation()
	{
		EngineContext context;
		SceneManager manager;
		ManagedScene sceneA = MakeScene(context, "CameraA", "SceneC");
		ManagedScene sceneB = MakeScene(context, "CameraB");
		ManagedScene sceneC = MakeScene(context, "CameraC");
		SceneBase* sceneBAddress = sceneB.scene.get();
		const CameraInfo* cameraB = sceneB.cameraInfo;

		manager.RegisterScene("SceneA", std::move(sceneA.scene));
		manager.RegisterScene("SceneB", std::move(sceneB.scene));
		manager.RegisterScene("SceneC", std::move(sceneC.scene));
		manager.SetInitialScene("SceneA");
		manager.Initialize(context);

		RedirectOnDestroy::destroyCount = 0;
		manager.ReserveChangeScene("SceneB");
		manager.Update(0.0f);
		Check(RedirectOnDestroy::destroyCount == 1,
			"Successful candidate swap finalizes the prior Scene exactly once");
		Check(manager.GetCameraInfo() == cameraB,
			"Successful candidate swap publishes the selected Scene");
		Check(manager.GetCurrentSceneName() == "SceneB",
			"A reserved transition retains its selected Scene name after reservation cleanup");

		manager.Update(0.0f);
		Check(manager.GetCameraInfo() == cameraB,
			"Prior Scene teardown cannot reserve an unintended follow-up transition");

		manager.SetViewportSize(800, 450);
		const std::filesystem::path missingPath = std::filesystem::temp_directory_path() /
			("101SceneManagerMissing-" + GuidGenerator::Generate().ToString() + ".scene");
		manager.RegisterSceneFile("BrokenScene", missingPath.string());
		manager.ReserveChangeScene("BrokenScene");
		manager.Update(0.0f);
		Check(manager.GetCameraInfo() == cameraB && manager.GetCurrentSceneName() == "SceneB" &&
			sceneBAddress->GetViewportSize().x == 800.0f &&
			sceneBAddress->GetViewportSize().y == 450.0f,
			"File candidate failure preserves the current Scene and viewport");

		manager.ReserveChangeScene("RegisteredLater");
		manager.Update(0.0f);
		ManagedScene registeredLater = MakeScene(context, "LateCamera");
		manager.RegisterScene("RegisteredLater", std::move(registeredLater.scene));
		manager.Update(0.0f);
		Check(manager.GetCameraInfo() == cameraB,
			"A missing Scene name completes its failed reservation without retrying");

		manager.ReserveChangeScene("SceneC");
		manager.Update(0.0f);
		manager.Finalize();
	}
}

int main()
{
	RegisterRedirectComponent();
	TestCandidateFailureAndTeardownReservationIsolation();
	ComponentRegistry::Get().UnregisterAllGameComponents();
	return failures ? 1 : 0;
}
