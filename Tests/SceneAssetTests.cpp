#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Camera.h"
#include "Engine/Project/ProjectSettings.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetReference.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneManager.h"
#include "Scene/SceneAssetWorkflow.h"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <Windows.h>

namespace
{
	int failures = 0;
	void Check(bool condition, const char* name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { ++failures; std::cerr << "[FAIL] " << name << '\n'; }
	}

	struct TemporaryDirectory
	{
		std::filesystem::path path = std::filesystem::temp_directory_path() /
			("101SceneAsset-" + std::to_string(GetCurrentProcessId()));
		TemporaryDirectory()
		{
			std::error_code error;
			std::filesystem::remove_all(path, error);
			std::filesystem::create_directories(path / "asset" / "scenes");
		}
		~TemporaryDirectory()
		{
			std::error_code error;
			std::filesystem::remove_all(path, error);
		}
	};

	std::unique_ptr<SceneBase> MakeScene(EngineContext& context)
	{
		auto scene = std::make_unique<SceneBase>();
		scene->Initialize(context);
		auto cameraActor = ActorFactory::CreateActor(
			ActorType::Camera, Actor::InitDesc(true, ActorTags::MainCamera, "Camera"));
		Camera* camera = cameraActor->GetComponentByClass<Camera>();
		scene->AddRootActor(std::move(cameraActor));
		scene->GetCameraSystem()->SetMainCamera(camera);
		return scene;
	}

	void TestSceneAssetLifecycle()
	{
		TemporaryDirectory temporary;
		AssetManager assets;
		Check(assets.Initialize((temporary.path / "asset").string(), nullptr, nullptr),
			"Empty Scene asset catalog initializes");
		EngineContext context;
		context.pAssetManager = &assets;
		auto scene = MakeScene(context);
		Guid guid;
		std::string path;
		SceneAssetWorkflowError error;
		Check(SceneAssetWorkflow::Create("Main.scene", *scene, assets, guid, path, &error),
			"Named Scene creation publishes a catalog asset");
		const AssetEntry* entry = assets.GetAssetEntry(guid);
		Check(entry && entry->type == AssetType::Scene,
			"Scene extension maps to AssetType::Scene");
		AssetReference<SceneAsset> reference;
		Check(reference.SetGuid(guid) && reference.GetExpectedType() == AssetType::Scene,
			"SceneAsset provides a type-safe AssetReference");
		SceneManager manager;
		Check(manager.SetInitialScene(guid), "SceneManager accepts a GUID Startup Scene");
		manager.Initialize(context);
		Check(manager.GetCurrentSceneAssetGuid() == guid && manager.GetCameraInfo(),
			"SceneManager loads a Scene through its catalog GUID");
		std::string renamedPath;
		Check(SceneAssetWorkflow::Rename(guid, "Renamed", assets, renamedPath, &error),
			"Scene rename publishes the new path");
		entry = assets.GetAssetEntry(guid);
		Check(entry && entry->relativePath == "scenes/Renamed.scene",
			"Scene rename preserves GUID and updates only catalog path");
		Check(std::filesystem::exists(renamedPath) &&
			std::filesystem::exists(renamedPath + ".meta"),
			"Scene and metadata move together");
		Check(manager.ReserveChangeScene(reference),
			"Typed Scene reference reserves a runtime transition");
		manager.Update(0.0f);
		Check(manager.GetCurrentSceneAssetGuid() == guid && manager.GetCameraInfo(),
			"Runtime transition resolves the renamed Scene's latest catalog path");
		manager.Finalize();
		scene->Finalize();

		SceneManager playManager;
		playManager.RegisterScene("EditorPlay", MakeScene(context));
		playManager.SetInitialScene("EditorPlay");
		playManager.Initialize(context);
		Check(playManager.GetCurrentScene() &&
			playManager.GetCurrentSceneName() == "EditorPlay",
			"Play manager owns the cloned runtime Scene");
		Check(playManager.ReserveChangeScene(guid),
			"Play runtime Scene can reserve an asset transition");
		playManager.Update(0.0f);
		Check(playManager.GetCurrentScene() &&
			playManager.GetCurrentSceneAssetGuid() == guid,
			"Play manager publishes the transitioned Scene asset");
		playManager.Finalize();
	}

	void TestProjectSettingsRoundTrip()
	{
		TemporaryDirectory temporary;
		const auto path = temporary.path / "project.101";
		{
			std::ofstream output(path);
			output << R"({"version":1,"custom":{"preserve":true}})";
		}
		ProjectSettings settings;
		std::string error;
		Check(ProjectSettings::Load(path, settings, &error),
			"Legacy project settings without Startup Scenes load");
		Guid guid;
		CoCreateGuid(&guid.value);
		settings.SetEditorStartupSceneGuid(guid);
		settings.SetGameStartupSceneGuid(guid);
		settings.SetUserTags({"Player", "Enemy"});
		Check(settings.Save(path, &error), "Project Startup Scene GUIDs save atomically");
		ProjectSettings restored;
		Check(ProjectSettings::Load(path, restored, &error) &&
			restored.GetEditorStartupSceneGuid() == guid &&
			restored.GetGameStartupSceneGuid() == guid &&
			restored.GetUserTags() == std::vector<std::string>({"Enemy", "Player"}),
			"Startup Scene GUIDs and sorted project Tags round trip");
		std::ifstream input(path);
		const auto json = nlohmann::json::parse(input);
		Check(json["custom"]["preserve"] == true,
			"Saving Startup Scenes preserves unrelated project settings");
		{
			std::ofstream output(path);
			output << R"({"tags":["Enemy","enemy"]})";
		}
		Check(!ProjectSettings::Load(path, restored, &error),
			"Project settings reject case-insensitive duplicate Tags");
	}
}

int main()
{
	TestSceneAssetLifecycle();
	TestProjectSettingsRoundTrip();
	return failures ? 1 : 0;
}
