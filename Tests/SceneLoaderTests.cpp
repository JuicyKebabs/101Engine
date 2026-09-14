#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Behavior.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/SkyRenderer.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Scene/SceneWriter.h"
#include "Engine/UI/Canvas.h"
#include "Engine/UI/UIImage.h"
#include "nlohmann/json.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

namespace
{
	using json = nlohmann::json;

	int g_failures = 0;
	int g_fileIndex = 0;

	class Version3TestBehavior final : public Behavior
	{
	public:
		float rotationSpeed = 1.0f;
		AssetReference<ActorImprint> actorImprint;
	};

	void RegisterVersion3TestBehavior()
	{
		TypeMetadataBuilder<Version3TestBehavior> builder("TestBehavior");
		builder.Property("rotationSpeed", &Version3TestBehavior::rotationSpeed);
		builder.Property("actorImprint", &Version3TestBehavior::actorImprint).Optional();
		ComponentRegistry::Get().RegisterGameComponent(
			"TestBehavior", []() -> Component* { return new Version3TestBehavior(); },
			typeid(Version3TestBehavior), std::make_unique<TypeMetadata>(*builder.Build()));
	}

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

	json MakeVersion4Scene(json actors = json::array(), json instances = json::array())
	{
		return {
			{ "version", 4 },
			{ "directional_light", {
				{ "direction", { 0.0, -1.0, 0.0 } },
				{ "color", { 1.0, 1.0, 1.0 } },
				{ "intensity", 1.0 }
			} },
			{ "actors", std::move(actors) },
			{ "actorImprintInstances", std::move(instances) },
		};
	}

	EngineContext& TestEngineContext()
	{
		static EngineContext context;
		return context;
	}

	void TestVersion3ChildBeforeParent()
	{
		const Guid parentGuid = GuidGenerator::Generate();
		const Guid childGuid = GuidGenerator::Generate();
		Transform transform;
		json transformData;
		transform.Serialize(transformData);
		TemporarySceneFile file(MakeVersion3Scene({
			MakeVersion3Actor(childGuid, "Child",
				json::array({ MakeComponentRecord("Transform", transformData) }), parentGuid.ToString()),
			MakeVersion3Actor(parentGuid, "Parent",
				json::array({ MakeComponentRecord("Transform", transformData) }))
		}));

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(static_cast<bool>(load), "Version 3 loads when a child appears before its parent");
		SceneBase* scene = load.scene.get();

		Actor* parent = scene ? scene->ResolveActor(parentGuid) : nullptr;
		Actor* child = scene ? scene->ResolveActor(childGuid) : nullptr;
		if (!parent || !child)
		{
			std::cerr << "[DIAG] expected parent=" << parentGuid.ToString()
				<< " child=" << childGuid.ToString()
				<< " actorCount=" << (scene ? scene->GetAllActors().size() : 0) << '\n';
			for (Actor* loaded : scene ? scene->GetAllActors() : std::vector<Actor*>{})
			{
				std::cerr << "[DIAG] loaded name=" << loaded->GetName()
					<< " guid=" << loaded->GetGuid().ToString() << '\n';
			}
		}
		Check(parent != nullptr && child != nullptr,
			"Version 3 preserves both persisted Guids");
		Check(child && child->GetParent() == parent,
			"Version 3 restores the child's parent reference");
		Check(parent && parent->GetDirectChildren().size() == 1 &&
			parent->GetDirectChildren().front() == child,
			"Version 3 restores the parent's child reference");
	}

	void TestRepositorySceneCompatibility()
	{
		std::ifstream stream("asset/scenes/test.scene");
		json repositoryScene = json::parse(stream);
		// Runtime asset loading is outside this loader compatibility test. Nulling
		// only asset values retains the real schema, hierarchy and component set.
		for (json& actor : repositoryScene["actors"])
		{
			for (json& component : actor["components"])
			{
				json& data = component["data"];
				if (data.contains("meshAssetId")) data["meshAssetId"] = nullptr;
				if (data.contains("textureAssetId")) data["textureAssetId"] = nullptr;
				if (data.contains("actorImprint")) data["actorImprint"] = nullptr;
			}
		}

		SceneLoadResult load = SceneLoader::LoadCandidate(
			repositoryScene, TestEngineContext(), "asset/scenes/test.scene");
		bool tagsPreserved = static_cast<bool>(load);
		if (load)
		{
			for (Actor* actor : load.scene->GetAllActors())
			{
				auto expected = std::find_if(repositoryScene["actors"].begin(),
					repositoryScene["actors"].end(), [actor](const json& record)
					{
						return record["actorId"].get<std::string>() == actor->GetGuid().ToString();
					});
				tagsPreserved = tagsPreserved && expected != repositoryScene["actors"].end() &&
					TagRegistry::Get().GetName(actor->GetTag()) == (*expected)["tag"].get<std::string>();
			}
		}
		Check(load && load.scene->GetAllActors().size() == repositoryScene["actors"].size() &&
			tagsPreserved,
			"The repository Scene schema and Actor set remain loadable");
		if (!load) std::cerr << "[DIAG] " << load.error.path << ": " << load.error.message << '\n';
		else if (!tagsPreserved || load.scene->GetAllActors().size() != repositoryScene["actors"].size())
			std::cerr << "[DIAG] actors=" << load.scene->GetAllActors().size()
				<< " expected=" << repositoryScene["actors"].size()
				<< " tagsPreserved=" << tagsPreserved << '\n';
		if (load.scene) load.scene->Finalize();
	}

	void TestVersion2Rejection()
	{
		TemporarySceneFile file(MakeVersion2Scene({}));
		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(!load && load.error.code == SceneLoadErrorCode::UnsupportedVersion &&
			load.error.path == "/version" && !load.error.message.empty(),
			"Version 2 is rejected with an explicit located diagnostic");
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

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(
			!load,
			"SceneLoader rejects unsupported Version 1 scenes");

		Check(
			!load.scene && load.error.code == SceneLoadErrorCode::UnsupportedVersion,
			"Version 1 rejection publishes no candidate Scene");
	}

	void TestVersion4StrictSchemaAndCandidateIsolation()
	{
		const std::string source = "memory://strict-v4.scene";
		json valid = MakeVersion4Scene();
		SceneLoadResult accepted = SceneLoader::LoadCandidate(valid, TestEngineContext(), source);
		Check(accepted && accepted.scene->GetAllActors().empty(),
			"Version 4 accepts required empty Actor arrays");

		auto ExpectFailure = [&](json invalid, SceneLoadErrorCode code,
			const char* path, const char* label)
		{
			SceneLoadResult load = SceneLoader::LoadCandidate(invalid, TestEngineContext(), source);
			Check(!load && !load.scene && load.error.code == code &&
				load.error.path == path && load.error.assetPath == source &&
				!load.error.message.empty(), label);
		};

		json unknown = valid;
		unknown["unexpected"] = true;
		ExpectFailure(unknown, SceneLoadErrorCode::InvalidSchema, "/unexpected",
			"Version 4 rejects an unknown top-level field with a complete diagnostic");
		json missingInstances = valid;
		missingInstances.erase("actorImprintInstances");
		ExpectFailure(missingInstances, SceneLoadErrorCode::InvalidSchema,
			"/actorImprintInstances", "Version 4 requires the Instance array even when empty");
		json wrongActors = valid;
		wrongActors["actors"] = nullptr;
		ExpectFailure(wrongActors, SceneLoadErrorCode::InvalidSchema, "/actors",
			"Version 4 rejects a null Actor array");
		json nullLight = valid;
		nullLight["directional_light"] = nullptr;
		ExpectFailure(nullLight, SceneLoadErrorCode::InvalidSceneSettings,
			"/directional_light", "Version 4 rejects null Scene settings");
		json outOfFloatRange = valid;
		outOfFloatRange["directional_light"]["intensity"] = 1.0e100;
		ExpectFailure(outOfFloatRange, SceneLoadErrorCode::InvalidSceneSettings,
			"/directional_light/intensity", "Version 4 rejects settings outside finite float range");
		json oversizedVersion = valid;
		oversizedVersion["version"] = std::uint64_t{ 4294967300ULL };
		ExpectFailure(oversizedVersion, SceneLoadErrorCode::UnsupportedVersion,
			"/version", "A large integer whose low bits equal 4 is not accepted as Version 4");
		json mislabeledVersion3 = valid;
		mislabeledVersion3["version"] = 3;
		mislabeledVersion3["actorImprintInstances"].push_back(json::object());
		ExpectFailure(mislabeledVersion3, SceneLoadErrorCode::InvalidSchema,
			"/actorImprintInstances", "Version 3 cannot silently discard Version 4 Instance records");

		TemporarySceneFile duplicateVersion(std::string(".duplicate-version.scene"));
		std::ofstream(duplicateVersion.String()) <<
			R"({"version":4,"directional_light":{"direction":[0.0,-1.0,0.0],"color":[1.0,1.0,1.0],"intensity":1.0},"actors":[],"actorImprintInstances":[],"version":3})";
		SceneLoadResult duplicateLoad = SceneLoader::LoadCandidate(
			duplicateVersion.String(), TestEngineContext());
		Check(!duplicateLoad && duplicateLoad.error.code == SceneLoadErrorCode::JsonParseFailed &&
			duplicateLoad.error.assetPath == duplicateVersion.String() &&
			duplicateLoad.error.message.find("version") != std::string::npos,
			"File loading rejects duplicate JSON fields before DOM normalization");

		Transform transform;
		json transformData;
		transform.Serialize(transformData);
		json actor = MakeVersion3Actor(GuidGenerator::Generate(), "StrictActor",
			json::array({ MakeComponentRecord("Transform", transformData) }));
		actor["unexpected"] = true;
		json nested = valid;
		nested["actors"].push_back(std::move(actor));
		ExpectFailure(nested, SceneLoadErrorCode::InvalidSchema,
			"/actors/0/unexpected", "Version 4 rejects an unknown nested Actor field");

		SceneBase current;
		current.Initialize(TestEngineContext());
		current.SetViewportSize(640, 360);
		Actor* sentinel = current.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Sentinel")));
		const Guid sentinelGuid = sentinel->GetGuid();
		SceneLoadResult rejected = SceneLoader::LoadCandidate(unknown, TestEngineContext(), source);
		Check(!rejected && current.ResolveActor(sentinelGuid) == sentinel &&
			current.GetViewportSize().x == 640.0f && current.GetViewportSize().y == 360.0f,
			"Candidate failure cannot mutate the caller's current Scene or viewport");

		TemporarySceneFile malformed(std::string(".malformed.scene"));
		std::ofstream(malformed.String()) << "{ invalid";
		SceneLoadResult parseFailure = SceneLoader::LoadCandidate(malformed.String(), TestEngineContext());
		Check(!parseFailure && parseFailure.error.code == SceneLoadErrorCode::JsonParseFailed &&
			parseFailure.error.assetPath == malformed.String() && !parseFailure.error.message.empty(),
			"Malformed Scene JSON returns a stable file diagnostic");
	}

	void TestDuplicateGuidRejection()
	{
		const Guid guid = GuidGenerator::Generate();
		Transform transform;
		json transformData;
		transform.Serialize(transformData);
		TemporarySceneFile file(MakeVersion3Scene({
			MakeVersion3Actor(guid, "First", json::array({ MakeComponentRecord("Transform", transformData) })),
			MakeVersion3Actor(guid, "Duplicate", json::array({ MakeComponentRecord("Transform", transformData) }))
		}));

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(!load && load.error.code == SceneLoadErrorCode::DuplicateActorGuid,
			"Version 3 rejects duplicate actor Guids");
		Check(!load.scene, "Duplicate Guid validation publishes no candidate");
	}

	void TestMissingParentRejection()
	{
		const Guid actorGuid = GuidGenerator::Generate();
		const Guid missingParentGuid = GuidGenerator::Generate();
		Transform transform;
		json transformData;
		transform.Serialize(transformData);
		TemporarySceneFile file(MakeVersion3Scene({
			MakeVersion3Actor(actorGuid, "Orphan",
				json::array({ MakeComponentRecord("Transform", transformData) }), missingParentGuid.ToString())
		}));

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(!load && load.error.code == SceneLoadErrorCode::InvalidHierarchy,
			"Version 3 rejects a missing parent Guid");
		Check(!load.scene, "Missing parent validation publishes no candidate");
	}

	void TestHierarchyCycleRejection()
	{
		const Guid firstGuid = GuidGenerator::Generate();
		const Guid secondGuid = GuidGenerator::Generate();
		Transform transform;
		json transformData;
		transform.Serialize(transformData);
		TemporarySceneFile file(MakeVersion3Scene({
			MakeVersion3Actor(firstGuid, "First",
				json::array({ MakeComponentRecord("Transform", transformData) }), secondGuid.ToString()),
			MakeVersion3Actor(secondGuid, "Second",
				json::array({ MakeComponentRecord("Transform", transformData) }), firstGuid.ToString())
		}));

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(!load && load.error.code == SceneLoadErrorCode::InvalidHierarchy,
			"Version 3 rejects a hierarchy cycle");
		Check(!load.scene, "Cycle validation publishes no candidate");
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
			"SceneWriter saves a temporary version 4 scene");

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(static_cast<bool>(load),
			"SceneLoader reloads the SceneWriter output");
		SceneBase* restored = load.scene.get();
		Actor* restoredCamera = restored ? restored->ResolveActor(cameraGuid) : nullptr;
		Actor* restoredChild = restored ? restored->ResolveActor(childGuid) : nullptr;
		Check(restoredCamera && restoredChild,
			"Writer-loader round trip preserves actor Guids");
		Check(restoredChild && restoredChild->GetParent() == restoredCamera,
			"Writer-loader round trip preserves hierarchy");
		Check(restored && restored->GetCameraSystem()->GetMainCamera() != nullptr,
			"Writer-loader round trip configures the main camera");

		DirectionalLight invalidLight = source.GetDirectionalLight();
		invalidLight.intensity = std::numeric_limits<float>::infinity();
		source.SetDirectionalLight(invalidLight);
		json unchanged = { { "sentinel", true } };
		Check(!SceneWriter::SerializeScene(&source, unchanged) && unchanged == json{ { "sentinel", true } },
			"SceneWriter rejects non-finite settings without modifying its output");
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
			"Version 4 writer saves Transform component data");

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(static_cast<bool>(load), "Version 4 loader restores Transform component data");

		Actor* restoredActor = load.scene ? load.scene->ResolveActor(actorGuid) : nullptr;
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

		RectTransform canvasTransform;
		json canvasTransformData;
		canvasTransform.Serialize(canvasTransformData);
		Canvas canvas;
		json canvasData;
		canvas.Serialize(canvasData);

		const Guid actorGuid = GuidGenerator::Generate();
		const Guid canvasGuid = GuidGenerator::Generate();
		const json sceneJson = MakeVersion3Scene({
			MakeVersion3Actor(
				actorGuid,
				"UIActor",
				json::array({ MakeComponentRecord("RectTransform", data) }),
				canvasGuid.ToString()),
			MakeVersion3Actor(
				canvasGuid,
				"CanvasActor",
				json::array({
					MakeComponentRecord(
						"RectTransform",
						canvasTransformData),
					MakeComponentRecord(
						"Canvas",
						canvasData)
				}))
		});
		TemporarySceneFile file(sceneJson);

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		const bool loaded = static_cast<bool>(load);
		Check(loaded,
			"Version 3 accepts RectTransform below a ScreenSpace Canvas");

		Actor* actor = load.scene ? load.scene->ResolveActor(actorGuid) : nullptr;
		Actor* canvasActor = load.scene ? load.scene->ResolveActor(canvasGuid) : nullptr;
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
			"UI hierarchy restoration keeps exactly one RectTransform");
		Check(
			rect &&
			actor->GetParent() == canvasActor &&
			rect->GetAnchorMode() == AnchorMode::BottomRight &&
			rect->GetAnchoredPosition().x == 18.0f &&
			rect->GetAnchoredPosition().y == -24.0f &&
			rect->GetPivot().x == 0.25f &&
			rect->GetPivot().y == 0.75f &&
			rect->GetSize().x == 320.0f &&
			rect->GetSize().y == 180.0f,
			"Deferred UI constraints preserve RectTransform values when the child record precedes its Canvas");
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
			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
			Check(!load,
				"Version 3 rejects an Actor without a Transform component");
			Check(!load.scene && load.error.code == SceneLoadErrorCode::ActorDeserializationFailed,
				"Missing Transform publishes no candidate Scene");
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
			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
			Check(!load,
				"Version 3 rejects multiple Transform-derived components");
			Check(!load.scene && load.error.code == SceneLoadErrorCode::ActorDeserializationFailed,
				"Duplicate Transform publishes no candidate Scene");
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
			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
			Check(!load,
				"Version 3 rejects an unregistered Component type");
			Check(!load.scene && load.error.path.find("/components/1/type") != std::string::npos,
				"Unknown Component failure is located and publishes no candidate");
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
			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
			Check(!load,
				"Version 3 rejects Component data that cannot be deserialized");
			Check(!load.scene && load.error.path.find("/components/0/data/position") != std::string::npos,
				"Deserialize failure is located and publishes no candidate");
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
		Check(source.SetCanvasRenderMode(
			canvas,
			CanvasRenderMode::WorldSpace),
			"Version 3 source configures Canvas render mode through Scene");

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
			"Version 4 writer saves referenced and Renderer components");

		SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
		Check(static_cast<bool>(load), "Version 4 loader restores referenced and Renderer components");
		SceneBase* restored = load.scene.get();

		Actor* restoredCameraActor = restored ? restored->ResolveActor(cameraGuid) : nullptr;
		Actor* restoredTargetActor = restored ? restored->ResolveActor(targetGuid) : nullptr;
		Actor* restoredMeshActor = restored ? restored->ResolveActor(meshGuid) : nullptr;
		Actor* restoredSpriteActor = restored ? restored->ResolveActor(spriteGuid) : nullptr;
		Actor* restoredCanvasActor = restored ? restored->ResolveActor(canvasGuid) : nullptr;
		Actor* restoredImageActor = restored ? restored->ResolveActor(imageGuid) : nullptr;

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
			"Version 4 restores every tested Component type");

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
		Check(restoredCanvas &&
			restoredCanvas->GetRenderMode() ==
				CanvasRenderMode::WorldSpace,
			"Version 3 preserves Canvas render mode");
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

			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
			Check(!load,
				"Version 3 rejects a missing Camera Actor reference");
			Check(!load.scene && load.error.code == SceneLoadErrorCode::ReferenceResolutionFailed,
				"Camera reference failure publishes no candidate Scene");
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

			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
			Check(!load,
				"Version 3 rejects a Canvas reference to an Actor without Canvas");
			Check(!load.scene && load.error.code == SceneLoadErrorCode::ReferenceResolutionFailed,
				"Canvas reference failure publishes no candidate Scene");
		}

		{
			MeshRenderer renderer;
			json rendererData;
			renderer.Serialize(rendererData);
			const Guid missingAssetGuid = GuidGenerator::Generate();
			rendererData["meshAssetId"] = missingAssetGuid.ToString();

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

			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
			Check(!load,
				"Version 3 propagates Asset reference resolution failure");
			Check(!load.scene && load.error.code == SceneLoadErrorCode::ReferenceResolutionFailed &&
				load.error.path == "/actors/0/components/1/data/meshAssetId" &&
				load.error.message.find(missingAssetGuid.ToString()) != std::string::npos &&
				load.error.message.find("expected Mesh") != std::string::npos &&
				load.error.message.find("not found") != std::string::npos,
				"Missing Asset failure retains its path, Guid, expected type and reason");
		}

		{
			MeshRenderer renderer;
			json rendererData;
			renderer.Serialize(rendererData);
			rendererData["meshAssetId"] = "not-a-guid";

			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(
					GuidGenerator::Generate(),
					"InvalidMeshGuid",
					json::array({
						MakeComponentRecord("Transform", transformData),
						MakeComponentRecord("MeshRenderer", rendererData)
					}))
			}));

			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), TestEngineContext());
			Check(!load.scene && load.error.path == "/actors/0/components/1/data/meshAssetId" &&
				load.error.message.find("invalid GUID") != std::string::npos,
				"Invalid Asset Guid reports its property path and representation failure");
		}

		{
			namespace fs = std::filesystem;
			const fs::path directory = fs::temp_directory_path() /
				("101Engine_SceneLoaderAssetType_" + GuidGenerator::Generate().ToString());
			fs::create_directories(directory);
			std::ofstream(directory / "texture.png").put('\0');

			AssetManager assetManager;
			const bool catalogReady = assetManager.Initialize(directory.string(), nullptr, nullptr);
			const AssetEntry* texture = assetManager.GetAssetEntryByPath("texture.png");
			MeshRenderer renderer;
			json rendererData;
			renderer.Serialize(rendererData);
			if (texture) rendererData["meshAssetId"] = texture->guid.ToString();

			TemporarySceneFile file(MakeVersion3Scene({
				MakeVersion3Actor(
					GuidGenerator::Generate(),
					"WrongMeshType",
					json::array({
						MakeComponentRecord("Transform", transformData),
						MakeComponentRecord("MeshRenderer", rendererData)
					}))
			}));
			EngineContext context{ .pAssetManager = &assetManager };
			SceneLoadResult load = SceneLoader::LoadCandidate(file.String(), context);
			Check(catalogReady && texture && !load.scene &&
				load.error.path == "/actors/0/components/1/data/meshAssetId" &&
				load.error.message.find(texture->guid.ToString()) != std::string::npos &&
				load.error.message.find("expected Mesh") != std::string::npos &&
				load.error.message.find("different Asset type") != std::string::npos,
				"Asset type mismatch retains its path, Guid, expected type and reason");

			std::error_code cleanupError;
			fs::remove_all(directory, cleanupError);
		}
	}

	void TestInitialSkySelection()
	{
		Transform transform;
		SkyRenderer sky;
		json transformData;
		json skyData;
		transform.Serialize(transformData);
		sky.Serialize(skyData);

		const Guid invalidTaggedId = GuidGenerator::Generate();
		const Guid firstSkyId = GuidGenerator::Generate();
		const Guid secondSkyId = GuidGenerator::Generate();
		json invalidTagged = MakeVersion3Actor(invalidTaggedId, "TaggedWithoutSky",
			json::array({ MakeComponentRecord("Transform", transformData) }));
		json firstSky = MakeVersion3Actor(firstSkyId, "FirstInitialSky",
			json::array({ MakeComponentRecord("Transform", transformData),
				MakeComponentRecord("SkyRenderer", skyData) }));
		json secondSky = MakeVersion3Actor(secondSkyId, "SecondInitialSky",
			json::array({ MakeComponentRecord("Transform", transformData),
				MakeComponentRecord("SkyRenderer", skyData) }));
		invalidTagged["tag"] = "InitialSky";
		firstSky["tag"] = "InitialSky";
		secondSky["tag"] = "InitialSky";

		TemporarySceneFile taggedFile(MakeVersion4Scene(
			json::array({ invalidTagged, firstSky, secondSky })));
		SceneLoadResult taggedLoad = SceneLoader::LoadCandidate(
			taggedFile.String(), TestEngineContext());
		SkyRenderer* active = taggedLoad.scene
			? taggedLoad.scene->GetRenderSystem()->GetActiveSkyRenderer() : nullptr;
		SkyRenderer* expected = nullptr;
		if (taggedLoad)
		{
			for (Actor* actor : taggedLoad.scene->GetAllActors())
			{
				if (actor->GetTag() != ActorTags::InitialSky) continue;
				expected = actor->GetComponentByClass<SkyRenderer>();
				if (expected) break;
			}
		}
		Check(taggedLoad && active && active == expected &&
			active->GetOwner()->GetGuid() != invalidTaggedId,
			"InitialSky skips invalid candidates and deterministically selects the first valid SkyRenderer");
		if (!taggedLoad) std::cerr << "[DIAG] " << taggedLoad.error.path << ": " << taggedLoad.error.message << '\n';
		else if (!active) std::cerr << "[DIAG] InitialSky load has no active SkyRenderer\n";

		json ordinarySky = firstSky;
		ordinarySky["tag"] = "None";
		TemporarySceneFile untaggedFile(MakeVersion4Scene(json::array({ ordinarySky })));
		SceneLoadResult untaggedLoad = SceneLoader::LoadCandidate(
			untaggedFile.String(), TestEngineContext());
		Check(untaggedLoad &&
			untaggedLoad.scene->GetRenderSystem()->GetActiveSkyRenderer() == nullptr,
			"A Scene without InitialSky loads without selecting an arbitrary SkyRenderer");
	}
}

int main()
{
	RegisterVersion3TestBehavior();
	TestVersion3ChildBeforeParent();
	TestRepositorySceneCompatibility();
	TestVersion2Rejection();
	TestVersion1Rejection();
	TestVersion4StrictSchemaAndCandidateIsolation();
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
	TestInitialSkySelection();
	ComponentRegistry::Get().UnregisterAllGameComponents();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " SceneLoader test(s) failed.\n";
		return 1;
	}

	std::cout << "All SceneLoader tests passed.\n";
	return 0;
}
