#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Scene/SceneWriter.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"
#include "nlohmann/json.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>

namespace
{
	using json = nlohmann::json;

	int g_failures = 0;
	int g_fileIndex = 0;

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

	class TemporarySceneFile
	{
	public:
		explicit TemporarySceneFile(const json& sceneJson)
			: m_path(std::filesystem::temp_directory_path() /
				("101Engine_SceneLoaderTest_" + std::to_string(++g_fileIndex) + ".scene"))
		{
			std::ofstream file(m_path);
			file << sceneJson.dump(2);
		}

		explicit TemporarySceneFile(std::string suffix)
			: m_path(std::filesystem::temp_directory_path() /
				("101Engine_SceneLoaderTest_" + std::to_string(++g_fileIndex) + suffix))
		{
		}

		~TemporarySceneFile()
		{
			std::error_code error;
			std::filesystem::remove(m_path, error);
		}

		std::string String() const { return m_path.string(); }

	private:
		std::filesystem::path m_path;
	};

	json MakeActor(const Guid& guid, const char* name, const json& parentId)
	{
		return {
			{"actorId", guid.ToString()},
			{"parentId", parentId},
			{"name", name},
			{"is_active", true},
			{"tag", "None"},
			{"components", json::array()}
		};
	}

	json MakeVersion2Scene(std::initializer_list<json> actors)
	{
		json actorArray = json::array();
		for (const json& actor : actors)
		{
			actorArray.push_back(actor);
		}

		return {
			{"version", 2},
			{"actors", std::move(actorArray)}
		};
	}

	json MakeComponentRecord(const char* type, const json& data)
	{
		return {
			{"type", type},
			{"data", data}
		};
	}

	json MakeVersion3Actor(
		const Guid& guid,
		const char* name,
		const json& components,
		const json& parentId = nullptr)
	{
		return {
			{"actorId", guid.ToString()},
			{"parentId", parentId},
			{"name", name},
			{"is_active", true},
			{"tag", "None"},
			{"components", components}
		};
	}

	json MakeVersion3Scene(std::initializer_list<json> actors)
	{
		json actorArray = json::array();
		for (const json& actor : actors)
		{
			actorArray.push_back(actor);
		}

		return {
			{"version", 3},
			{"actors", std::move(actorArray)}
		};
	}

	void TestVersion2ChildBeforeParent()
	{
		const Guid parentGuid = GuidGenerator::Generate();
		const Guid childGuid = GuidGenerator::Generate();
		TemporarySceneFile file(MakeVersion2Scene({
			MakeActor(childGuid, "Child", parentGuid.ToString()),
			MakeActor(parentGuid, "Parent", nullptr)
		}));

		SceneBase scene;
		Check(SceneLoader::LoadScene(file.String(), &scene),
			"Version 2 loads when a child appears before its parent");

		Actor* parent = scene.ResolveActor(parentGuid);
		Actor* child = scene.ResolveActor(childGuid);
		if (!parent || !child)
		{
			std::cerr << "[DIAG] expected parent=" << parentGuid.ToString()
				<< " child=" << childGuid.ToString()
				<< " actorCount=" << scene.GetAllActors().size() << '\n';
			for (Actor* loaded : scene.GetAllActors())
			{
				std::cerr << "[DIAG] loaded name=" << loaded->GetName()
					<< " guid=" << loaded->GetGuid().ToString() << '\n';
			}
		}
		Check(parent != nullptr && child != nullptr,
			"Version 2 preserves both persisted Guids");
		Check(child && child->GetParent() == parent,
			"Version 2 restores the child's parent reference");
		Check(parent && parent->GetDirectChildren().size() == 1 &&
			parent->GetDirectChildren().front() == child,
			"Version 2 restores the parent's child reference");
	}

	void TestVersion1Rejection()
	{
		const json legacyActor = {
			{"name", "LegacyActor"},
			{"is_active", true},
			{"tag", "None"},
			{"components", json::array()}
		};

		TemporarySceneFile file({
			{"version", 1},
			{"actors", json::array({ legacyActor })}
			});

		SceneBase scene;

		Check(
			!SceneLoader::LoadScene(file.String(), &scene),
			"SceneLoader rejects unsupported Version 1 scenes");

		Check(
			scene.GetAllActors().empty(),
			"Version 1 rejection does not create Actors");
	}

	void TestDuplicateGuidRejection()
	{
		const Guid guid = GuidGenerator::Generate();
		TemporarySceneFile file(MakeVersion2Scene({
			MakeActor(guid, "First", nullptr),
			MakeActor(guid, "Duplicate", nullptr)
		}));

		SceneBase scene;
		Check(!SceneLoader::LoadScene(file.String(), &scene),
			"Version 2 rejects duplicate actor Guids");
		Check(scene.GetAllActors().empty(),
			"Duplicate Guid validation fails before actor creation");
	}

	void TestMissingParentRejection()
	{
		const Guid actorGuid = GuidGenerator::Generate();
		const Guid missingParentGuid = GuidGenerator::Generate();
		TemporarySceneFile file(MakeVersion2Scene({
			MakeActor(actorGuid, "Orphan", missingParentGuid.ToString())
		}));

		SceneBase scene;
		Check(!SceneLoader::LoadScene(file.String(), &scene),
			"Version 2 rejects a missing parent Guid");
		Check(scene.GetAllActors().empty(),
			"Missing parent validation fails before actor creation");
	}

	void TestHierarchyCycleRejection()
	{
		const Guid firstGuid = GuidGenerator::Generate();
		const Guid secondGuid = GuidGenerator::Generate();
		TemporarySceneFile file(MakeVersion2Scene({
			MakeActor(firstGuid, "First", secondGuid.ToString()),
			MakeActor(secondGuid, "Second", firstGuid.ToString())
		}));

		SceneBase scene;
		Check(!SceneLoader::LoadScene(file.String(), &scene),
			"Version 2 rejects a hierarchy cycle");
		Check(scene.GetAllActors().empty(),
			"Cycle validation fails before actor creation");
	}

	void TestWriterLoaderRoundTrip()
	{
		SceneBase source;
		auto cameraOwned = ActorFactory::CreateActor(
			ActorType::Camera,
			Actor::InitDesc(true, ActorTags::MainCamera, "MainCamera"));
		Actor* camera = source.AddRootActor(std::move(cameraOwned));
		auto childOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Child"));
		Actor* child = source.AddChildActor(std::move(childOwned), camera->GetHandle());
		const Guid cameraGuid = camera->GetGuid();
		const Guid childGuid = child->GetGuid();

		TemporarySceneFile file(std::string(".roundtrip.scene"));
		Check(SceneWriter::SaveScene(file.String(), &source),
			"SceneWriter saves a temporary version 3 scene");

		SceneBase restored;
		Check(SceneLoader::LoadScene(file.String(), &restored),
			"SceneLoader reloads the SceneWriter output");
		Actor* restoredCamera = restored.ResolveActor(cameraGuid);
		Actor* restoredChild = restored.ResolveActor(childGuid);
		Check(restoredCamera && restoredChild,
			"Writer-loader round trip preserves actor Guids");
		Check(restoredChild && restoredChild->GetParent() == restoredCamera,
			"Writer-loader round trip preserves hierarchy");
		Check(restored.GetCameraSystem()->GetMainCamera() != nullptr,
			"Writer-loader round trip configures the main camera");
	}

	void TestVersion3TransformDataRoundTrip()
	{
		SceneBase source;
		auto cameraOwned = ActorFactory::CreateActor(
			ActorType::Camera,
			Actor::InitDesc(true, ActorTags::MainCamera, "MainCamera"));
		Actor* camera = source.AddRootActor(std::move(cameraOwned));
		auto actorOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Transformed"));
		Actor* actor = source.AddRootActor(std::move(actorOwned));
		Transform* transform = actor->GetComponentByClass<Transform>();
		transform->SetLocalPosition({ 1.25f, -2.5f, 3.75f });
		transform->SetLocalRotationQuat({ 0.0f, 0.0f, 0.70710677f, 0.70710677f });
		transform->SetLocalScale({ 2.0f, 3.0f, 4.0f });
		const Guid actorGuid = actor->GetGuid();

		TemporarySceneFile file(std::string(".v3-transform.scene"));
		Check(camera && SceneWriter::SaveScene(file.String(), &source),
			"Version 3 writer saves Transform component data");

		SceneBase restored;
		Check(SceneLoader::LoadScene(file.String(), &restored),
			"Version 3 loader restores Transform component data");

		Actor* restoredActor = restored.ResolveActor(actorGuid);
		Transform* restoredTransform = restoredActor
			? restoredActor->GetComponentByClass<Transform>()
			: nullptr;
		if (!restoredTransform)
		{
			Check(false, "Version 3 round trip retains the Transform component");
			return;
		}

		const Vector3 position = restoredTransform->GetLocalPosition();
		const Quaternion rotation = restoredTransform->GetLocalRotationQuat();
		const Vector3 scale = restoredTransform->GetLocalScale();
		constexpr float epsilon = 0.0001f;
		Check(
			std::abs(position.x - 1.25f) < epsilon &&
			std::abs(position.y + 2.5f) < epsilon &&
			std::abs(position.z - 3.75f) < epsilon &&
			std::abs(rotation.z - 0.70710677f) < epsilon &&
			std::abs(rotation.w - 0.70710677f) < epsilon &&
			std::abs(scale.x - 2.0f) < epsilon &&
			std::abs(scale.y - 3.0f) < epsilon &&
			std::abs(scale.z - 4.0f) < epsilon,
			"Version 3 round trip preserves Transform values");
	}

	void TestVersion3RectTransformWithoutBaseTransform()
	{
		RectTransform source;
		source.SetAnchorMode(AnchorMode::BottomRight);
		source.SetAnchoredPosition({ 18.0f, -24.0f });
		source.SetPivot({ 0.25f, 0.75f });
		source.SetSizeDelta({ 320.0f, 180.0f });
		json data;
		source.Serialize(data);

		const Guid actorGuid = GuidGenerator::Generate();
		const json sceneJson = MakeVersion3Scene({
			MakeVersion3Actor(
				actorGuid,
				"UIActor",
				json::array({ MakeComponentRecord("RectTransform", data) }))
		});
		TemporarySceneFile file(sceneJson);

		SceneBase scene;
		const bool loaded = SceneLoader::LoadScene(file.String(), &scene);
		Check(loaded,
			"Version 3 accepts RectTransform as the Actor transform");

		Actor* actor = scene.ResolveActor(actorGuid);
		RectTransform* rect = actor
			? actor->GetComponentByClass<RectTransform>()
			: nullptr;
		int exactTransformCount = 0;
		int exactRectTransformCount = 0;
		if (actor)
		{
			for (Component* component : actor->GetAllComponents())
			{
				if (typeid(*component) == typeid(Transform))
				{
					++exactTransformCount;
				}
				if (typeid(*component) == typeid(RectTransform))
				{
					++exactRectTransformCount;
				}
			}
		}

		Check(
			rect &&
			exactTransformCount == 0 &&
			exactRectTransformCount == 1,
			"RectTransform restoration does not add a base Transform");
		Check(
			rect &&
			rect->GetAnchorMode() == AnchorMode::BottomRight &&
			rect->GetAnchoredPosition().x == 18.0f &&
			rect->GetAnchoredPosition().y == -24.0f &&
			rect->GetPivot().x == 0.25f &&
			rect->GetPivot().y == 0.75f &&
			rect->GetSize().x == 320.0f &&
			rect->GetSize().y == 180.0f,
			"Version 3 preserves RectTransform values");
	}

	void TestVersion3TransformValidation()
	{
		Transform transform;
		json transformData;
		transform.Serialize(transformData);
		RectTransform rectTransform;
		json rectTransformData;
		rectTransform.Serialize(rectTransformData);

		{
			const Guid actorGuid = GuidGenerator::Generate();
			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(actorGuid, "MissingTransform", json::array())
			}));
			SceneBase scene;
			Check(!SceneLoader::LoadScene(file.String(), &scene),
				"Version 3 rejects an Actor without a Transform component");
			Check(scene.GetAllActors().empty(),
				"Missing Transform fails before the Actor enters the Scene");
		}

		{
			const Guid actorGuid = GuidGenerator::Generate();
			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(
					actorGuid,
					"DuplicateTransform",
					json::array({
						MakeComponentRecord("Transform", transformData),
						MakeComponentRecord("RectTransform", rectTransformData)
					}))
			}));
			SceneBase scene;
			Check(!SceneLoader::LoadScene(file.String(), &scene),
				"Version 3 rejects multiple Transform-derived components");
			Check(scene.GetAllActors().empty(),
				"Duplicate Transform validation fails before Scene registration");
		}
	}

	void TestVersion3InvalidComponentRejection()
	{
		Transform transform;
		json transformData;
		transform.Serialize(transformData);

		{
			const Guid actorGuid = GuidGenerator::Generate();
			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(
					actorGuid,
					"UnknownComponent",
					json::array({
						MakeComponentRecord("Transform", transformData),
						MakeComponentRecord("NotRegistered", json::object())
					}))
			}));
			SceneBase scene;
			Check(!SceneLoader::LoadScene(file.String(), &scene),
				"Version 3 rejects an unregistered Component type");
			Check(scene.GetAllActors().empty(),
				"Unknown Component fails before Actor registration");
		}

		{
			json invalidTransformData = transformData;
			invalidTransformData["position"] = "invalid";
			const Guid actorGuid = GuidGenerator::Generate();
			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(
					actorGuid,
					"InvalidComponentData",
					json::array({
						MakeComponentRecord("Transform", invalidTransformData)
					}))
			}));
			SceneBase scene;
			Check(!SceneLoader::LoadScene(file.String(), &scene),
				"Version 3 rejects Component data that cannot be deserialized");
			Check(scene.GetAllActors().empty(),
				"Deserialize failure does not register the Actor");
		}
	}

	void TestVersion3ComponentReferencesAndRendererRoundTrip()
	{
		SceneBase source;

		auto cameraOwned = ActorFactory::CreateActor(
			ActorType::Camera,
			Actor::InitDesc(true, ActorTags::MainCamera, "MainCamera"));
		Actor* cameraActor = source.AddRootActor(std::move(cameraOwned));

		auto targetOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "CameraTarget"));
		Actor* targetActor = source.AddRootActor(std::move(targetOwned));

		auto meshOwned = ActorFactory::CreateActor(
			ActorType::Mesh,
			Actor::InitDesc(true, TAG_NONE, "MeshActor"));
		Actor* meshActor = source.AddRootActor(std::move(meshOwned));

		auto spriteOwned = ActorFactory::CreateActor(
			ActorType::Sprite,
			Actor::InitDesc(true, TAG_NONE, "SpriteActor"));
		Actor* spriteActor = source.AddRootActor(std::move(spriteOwned));

		auto canvasOwned = ActorFactory::CreateActor(
			ActorType::Canvas,
			Actor::InitDesc(true, TAG_NONE, "CanvasActor"));
		Actor* canvasActor = source.AddRootActor(std::move(canvasOwned));

		auto imageOwned = ActorFactory::CreateActor(
			ActorType::UI,
			Actor::InitDesc(true, TAG_NONE, "ImageActor"));
		Actor* imageActor = source.AddChildActor(
			std::move(imageOwned),
			canvasActor->GetHandle());

		Camera* camera = cameraActor->GetComponentByClass<Camera>();
		MeshRenderer* mesh = meshActor->GetComponentByClass<MeshRenderer>();
		SpriteRenderer* sprite =
			spriteActor->GetComponentByClass<SpriteRenderer>();
		Canvas* canvas = canvasActor->GetComponentByClass<Canvas>();
		UIImage* image = imageActor->GetComponentByClass<UIImage>();

		camera->SetTargetActor(targetActor);
		camera->SetFollowTarget(targetActor);

		mesh->SetColor({ 0.1f, 0.2f, 0.3f, 0.4f });
		mesh->SetVisible(false);

		sprite->SetColor({ 0.6f, 0.7f, 0.8f, 0.9f });
		sprite->SetVisible(false);
		sprite->SetUVScale({ 2.0f, 3.0f });
		sprite->SetUVOffset({ 0.25f, 0.5f });
		sprite->SetFlipX(true);
		sprite->SetFlipY(true);

		canvas->SetSortOrder(12);
		canvas->SetVisible(false);

		image->SetCanvas(canvas);
		image->SetOrder(7);
		image->SetColor({ 0.9f, 0.8f, 0.7f, 0.6f });
		image->SetVisible(false);
		image->SetUVScale({ 4.0f, 5.0f });
		image->SetUVOffset({ 0.125f, 0.375f });
		image->SetFlipX(true);

		json expectedCamera;
		json expectedMesh;
		json expectedSprite;
		json expectedCanvas;
		json expectedImage;
		camera->Serialize(expectedCamera);
		mesh->Serialize(expectedMesh);
		sprite->Serialize(expectedSprite);
		canvas->Serialize(expectedCanvas);
		image->Serialize(expectedImage);

		const Guid cameraGuid = cameraActor->GetGuid();
		const Guid targetGuid = targetActor->GetGuid();
		const Guid meshGuid = meshActor->GetGuid();
		const Guid spriteGuid = spriteActor->GetGuid();
		const Guid canvasGuid = canvasActor->GetGuid();
		const Guid imageGuid = imageActor->GetGuid();

		TemporarySceneFile file(std::string(".v3-components.scene"));
		Check(SceneWriter::SaveScene(file.String(), &source),
			"Version 3 writer saves referenced and Renderer components");

		SceneBase restored;
		Check(SceneLoader::LoadScene(file.String(), &restored),
			"Version 3 loader restores referenced and Renderer components");

		Actor* restoredCameraActor = restored.ResolveActor(cameraGuid);
		Actor* restoredTargetActor = restored.ResolveActor(targetGuid);
		Actor* restoredMeshActor = restored.ResolveActor(meshGuid);
		Actor* restoredSpriteActor = restored.ResolveActor(spriteGuid);
		Actor* restoredCanvasActor = restored.ResolveActor(canvasGuid);
		Actor* restoredImageActor = restored.ResolveActor(imageGuid);

		Camera* restoredCamera = restoredCameraActor
			? restoredCameraActor->GetComponentByClass<Camera>()
			: nullptr;
		MeshRenderer* restoredMesh = restoredMeshActor
			? restoredMeshActor->GetComponentByClass<MeshRenderer>()
			: nullptr;
		SpriteRenderer* restoredSprite = restoredSpriteActor
			? restoredSpriteActor->GetComponentByClass<SpriteRenderer>()
			: nullptr;
		Canvas* restoredCanvas = restoredCanvasActor
			? restoredCanvasActor->GetComponentByClass<Canvas>()
			: nullptr;
		UIImage* restoredImage = restoredImageActor
			? restoredImageActor->GetComponentByClass<UIImage>()
			: nullptr;

		Check(
			restoredCamera &&
			restoredTargetActor &&
			restoredMesh &&
			restoredSprite &&
			restoredCanvas &&
			restoredImage,
			"Version 3 restores every tested Component type");

		json actualCamera;
		json actualMesh;
		json actualSprite;
		json actualCanvas;
		json actualImage;
		if (restoredCamera) restoredCamera->Serialize(actualCamera);
		if (restoredMesh) restoredMesh->Serialize(actualMesh);
		if (restoredSprite) restoredSprite->Serialize(actualSprite);
		if (restoredCanvas) restoredCanvas->Serialize(actualCanvas);
		if (restoredImage) restoredImage->Serialize(actualImage);

		Check(
			restoredCamera &&
			actualCamera == expectedCamera &&
			actualCamera["targetActorId"] == targetGuid.ToString() &&
			actualCamera["followActorId"] == targetGuid.ToString(),
			"Version 3 resolves and preserves Camera Actor references");
		Check(restoredMesh && actualMesh == expectedMesh,
			"Version 3 preserves MeshRenderer settings");
		Check(restoredSprite && actualSprite == expectedSprite,
			"Version 3 preserves SpriteRenderer settings");
		Check(restoredCanvas && actualCanvas == expectedCanvas,
			"Version 3 preserves Canvas settings");
		Check(
			restoredImage &&
			actualImage == expectedImage &&
			restoredImage->GetCanvas() == restoredCanvas,
			"Version 3 resolves Canvas references and preserves UIImage settings");
		Check(
			restoredImageActor &&
			restoredImageActor->GetParent() == restoredCanvasActor,
			"Version 3 preserves UI hierarchy alongside Canvas references");
	}

	void TestVersion3ReferenceResolutionFailures()
	{
		Transform transform;
		json transformData;
		transform.Serialize(transformData);

		{
			Camera camera;
			json cameraData;
			camera.Serialize(cameraData);
			cameraData["targetActorId"] =
				GuidGenerator::Generate().ToString();

			const Guid cameraGuid = GuidGenerator::Generate();
			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(
					cameraGuid,
					"MissingCameraTarget",
					json::array({
						MakeComponentRecord("Transform", transformData),
						MakeComponentRecord("Camera", cameraData)
					}))
			}));

			SceneBase scene;
			Check(!SceneLoader::LoadScene(file.String(), &scene),
				"Version 3 rejects a missing Camera Actor reference");
			Check(scene.ResolveActor(cameraGuid) != nullptr,
				"Camera reference failure occurs after Actor creation");
		}

		{
			RectTransform rectTransform;
			json rectData;
			rectTransform.Serialize(rectData);
			UIImage image;
			json imageData;
			image.Serialize(imageData);

			const Guid nonCanvasGuid = GuidGenerator::Generate();
			const Guid imageGuid = GuidGenerator::Generate();
			imageData["canvasActorId"] = nonCanvasGuid.ToString();

			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(
					nonCanvasGuid,
					"NotACanvas",
					json::array({
						MakeComponentRecord("Transform", transformData)
					})),
				MakeVersion3Actor(
					imageGuid,
					"InvalidCanvasImage",
					json::array({
						MakeComponentRecord("RectTransform", rectData),
						MakeComponentRecord("UIImage", imageData)
					}))
			}));

			SceneBase scene;
			Check(!SceneLoader::LoadScene(file.String(), &scene),
				"Version 3 rejects a Canvas reference to an Actor without Canvas");
			Check(
				scene.ResolveActor(nonCanvasGuid) != nullptr &&
				scene.ResolveActor(imageGuid) != nullptr,
				"Canvas reference failure occurs after all Actors are created");
		}

		{
			MeshRenderer renderer;
			json rendererData;
			renderer.Serialize(rendererData);
			rendererData["meshAssetId"] =
				GuidGenerator::Generate().ToString();

			const Guid actorGuid = GuidGenerator::Generate();
			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(
					actorGuid,
					"MissingMeshContext",
					json::array({
						MakeComponentRecord("Transform", transformData),
						MakeComponentRecord("MeshRenderer", rendererData)
					}))
			}));

			SceneBase scene;
			Check(!SceneLoader::LoadScene(file.String(), &scene),
				"Version 3 propagates Asset reference resolution failure");
			Check(scene.ResolveActor(actorGuid) != nullptr,
				"Asset resolution failure occurs after Actor creation");
		}
	}
}

int main()
{
	TestVersion2ChildBeforeParent();
	TestVersion1Rejection();
	TestDuplicateGuidRejection();
	TestMissingParentRejection();
	TestHierarchyCycleRejection();
	TestWriterLoaderRoundTrip();
	TestVersion3TransformDataRoundTrip();
	TestVersion3RectTransformWithoutBaseTransform();
	TestVersion3TransformValidation();
	TestVersion3InvalidComponentRejection();
	TestVersion3ComponentReferencesAndRendererRoundTrip();
	TestVersion3ReferenceResolutionFailures();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " SceneLoader test(s) failed.\n";
		return 1;
	}

	std::cout << "All SceneLoader tests passed.\n";
	return 0;
}
