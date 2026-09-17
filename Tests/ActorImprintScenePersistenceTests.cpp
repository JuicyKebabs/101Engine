#include "Scene/SceneCloner.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorReference.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/ActorImprint/ActorImprintInstanceRegistry.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Scene/SceneWriter.h"
#include "Engine/UI/Canvas.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace
{
	using json = nlohmann::json;
	int failures = 0;

	void Check(bool condition, const char* name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { ++failures; std::cerr << "[FAIL] " << name << '\n'; }
	}

	class SceneProbe final : public Component
	{
	public:
		ActorReference target;
		float weight = 1.0f;
		static inline int attachCount = 0;
		static inline int destroyCount = 0;

		bool ResolveReferences(SceneBase& scene) override
		{
			return !target.HasValue() || target.Resolve(scene) != nullptr;
		}

	private:
		void OnAttachOverride() override { ++attachCount; }
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override { ++destroyCount; }
	};

	void RegisterProbe()
	{
		TypeMetadataBuilder<SceneProbe> builder("ActorImprintSceneProbeET13");
		builder.Property("target", &SceneProbe::target);
		builder.Property("weight", &SceneProbe::weight);
		ComponentRegistry::Get().RegisterGameComponent(
			"ActorImprintSceneProbeET13",
			[]() -> Component* { return new SceneProbe(); },
			typeid(SceneProbe),
			std::make_unique<TypeMetadata>(*builder.Build()));
	}

	json BuildDefinition()
	{
		std::ifstream stream("Tests/Fixtures/ActorImprint/Minimal.imprint");
		json definition = json::parse(stream);
		json child = definition["actors"][0];
		child["localObjectId"] = 20;
		child["parentLocalObjectId"] = 10;
		child["properties"]["name"] = "Child";
		child["components"][0]["localObjectId"] = 21;
		definition["actors"].push_back(std::move(child));
		definition["actors"][0]["components"].push_back({
			{ "localObjectId", 12 },
			{ "type", "ActorImprintSceneProbeET13" },
			{ "properties", {
				{ "target", nullptr },
				{ "weight", 1.0 }
			} }
		});
		definition["nextLocalObjectId"] = 30;
		return definition;
	}

	struct Fixture
	{
		std::filesystem::path path = std::filesystem::temp_directory_path() /
			("101ImprintScenePersistence-" + GuidGenerator::Generate().ToString());
		Guid assetGuid = GuidGenerator::Generate();
		AssetManager assets;
		ActorImprintSystem system{ assets };
		EngineContext context{};
		ActorImprintHandle imprint;

		Fixture()
		{
			std::filesystem::create_directories(path);
			std::ofstream(path / "scene-test.imprint") << BuildDefinition();
			MetaFile::Save((path / "scene-test.imprint").string(), assetGuid);
			Check(assets.Initialize(path.string(), nullptr, nullptr),
				"Scene persistence fixture catalog initializes");
			context.pAssetManager = &assets;
			context.pActorImprintSystem = &system;
			imprint = system.Load(assetGuid);
			Check(!imprint.IsNull(), "Scene persistence fixture definition loads");
		}

		~Fixture()
		{
			if (std::filesystem::absolute(path).parent_path() ==
				std::filesystem::absolute(std::filesystem::temp_directory_path()))
				std::filesystem::remove_all(path);
		}
	};

	struct SceneIds
	{
		Guid camera;
		Guid parent;
		Guid rootA;
		Guid childA;
		Guid rootB;
		Guid childB;
	};

	bool PopulateScene(Fixture& fixture, SceneBase& scene, SceneIds& ids)
	{
		scene.Initialize(fixture.context);
		auto cameraOwned = ActorFactory::CreateActor(
			ActorType::Camera, Actor::InitDesc(true, ActorTags::MainCamera, "MainCamera"));
		Camera* camera = cameraOwned ? cameraOwned->GetComponentByClass<Camera>() : nullptr;
		Actor* cameraActor = scene.AddRootActor(std::move(cameraOwned));
		Actor* parent = scene.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "OrdinaryParent")));
		if (!cameraActor || !camera || !parent) return false;

		Actor* rootA = fixture.system.Instantiate(scene, fixture.imprint, parent->GetHandle());
		Actor* rootB = fixture.system.Instantiate(scene, fixture.imprint, parent->GetHandle());
		if (!rootA || !rootB) return false;
		Actor* childA = scene.GetImprintInstances().ResolveActor(rootA->GetHandle(), 20);
		Actor* childB = scene.GetImprintInstances().ResolveActor(rootB->GetHandle(), 20);
		auto* probeA = static_cast<SceneProbe*>(
			scene.GetImprintInstances().ResolveComponent(rootA->GetHandle(), 12));
		auto* probeB = static_cast<SceneProbe*>(
			scene.GetImprintInstances().ResolveComponent(rootB->GetHandle(), 12));
		if (!childA || !childB || !probeA || !probeB) return false;
		if (!probeA->target.Set(childB) || !probeB->target.Set(childA)) return false;
		probeA->weight = 2.0f;
		probeB->weight = 3.0f;
		rootA->SetName("InstanceA");
		rootB->SetName("InstanceB");
		if (!camera->SetTargetActor(childA)) return false;
		scene.GetCameraSystem()->SetMainCamera(camera);

		ids = {
			cameraActor->GetGuid(), parent->GetGuid(), rootA->GetGuid(), childA->GetGuid(),
			rootB->GetGuid(), childB->GetGuid()
		};
		return true;
	}

	bool BuildSerializedScene(Fixture& fixture, json& outJson, SceneIds& outIds)
	{
		SceneBase source;
		if (!PopulateScene(fixture, source, outIds)) return false;
		const bool serialized = SceneWriter::SerializeScene(&source, outJson);
		source.Finalize();
		return serialized;
	}

	json* FindInstance(json& scene, const Guid& rootGuid)
	{
		for (json& instance : scene["actorImprintInstances"])
			if (instance["rootActorGuid"] == rootGuid.ToString()) return &instance;
		return nullptr;
	}

	json* FindMapping(json& instance, LocalObjectId id)
	{
		for (json& mapping : instance["actorGuids"])
			if (mapping["localObjectId"] == id) return &mapping;
		return nullptr;
	}

	json* FindOverride(json& instance, LocalObjectId id)
	{
		if (!instance.contains("propertyOverrides")) return nullptr;
		for (json& target : instance["propertyOverrides"])
			if (target["targetLocalObjectId"] == id) return &target;
		return nullptr;
	}

	bool VerifyLoadedScene(SceneBase& scene, const SceneIds& ids)
	{
		Actor* cameraActor = scene.ResolveActor(ids.camera);
		Actor* parent = scene.ResolveActor(ids.parent);
		Actor* rootA = scene.ResolveActor(ids.rootA);
		Actor* rootB = scene.ResolveActor(ids.rootB);
		Actor* childA = scene.ResolveActor(ids.childA);
		Actor* childB = scene.ResolveActor(ids.childB);
		if (!cameraActor || !parent || !rootA || !rootB || !childA || !childB) return false;
		const auto& registry = scene.GetImprintInstances();
		auto* probeA = static_cast<SceneProbe*>(registry.ResolveComponent(rootA->GetHandle(), 12));
		auto* probeB = static_cast<SceneProbe*>(registry.ResolveComponent(rootB->GetHandle(), 12));
		Camera* camera = cameraActor->GetComponentByClass<Camera>();
		return scene.GetAllActors().size() == 6 && registry.GetInstances().size() == 2 &&
			parent->GetTag() == TAG_NONE &&
			rootA->GetParent() == parent && rootB->GetParent() == parent &&
			registry.ResolveActor(rootA->GetHandle(), 20) == childA &&
			registry.ResolveActor(rootB->GetHandle(), 20) == childB &&
			probeA && probeB && probeA->weight == 2.0f && probeB->weight == 3.0f &&
			probeA->target.Resolve(scene) == childB && probeB->target.Resolve(scene) == childA &&
			probeA->IsAttached() && probeB->IsAttached() && camera &&
			camera->GetTargetActorReference().Resolve(scene) == childA;
	}

	void TestVersion4RoundTripOrderAndDeterminism()
	{
		Fixture fixture;
		json saved;
		SceneIds ids;
		Check(BuildSerializedScene(fixture, saved, ids),
			"Version 4 source Scene with two Instances serializes");
		Check(saved["version"] == 4 &&
			saved["actors"].size() == 2 &&
			saved["actorImprintInstances"].size() == 2,
			"Version 4 separates ordinary Actors from Instance records");
		bool instanceActorLeaked = false;
		for (const json& actor : saved["actors"])
		{
			const std::string guid = actor["actorId"].get<std::string>();
			instanceActorLeaked = instanceActorLeaked || guid == ids.rootA.ToString() ||
				guid == ids.childA.ToString() || guid == ids.rootB.ToString() || guid == ids.childB.ToString();
		}
		Check(!instanceActorLeaked, "Instance members are never duplicated in the ordinary Actor array");

		SceneLoadResult first = SceneLoader::LoadCandidate(saved, fixture.context, "memory://ordered.scene");
		Check(first &&
			VerifyLoadedScene(*first.scene, ids),
			"Whole-Scene candidate resolves ordinary-to-Instance and mutual Instance references");
		json canonicalFirst;
		Check(first &&
			SceneWriter::SerializeScene(first.scene.get(), canonicalFirst) &&
			canonicalFirst.dump(4) == saved.dump(4),
			"Save-Load-Save produces the same canonical JSON bytes");

		json reordered = saved;
		std::reverse(reordered["actors"].begin(), reordered["actors"].end());
		std::reverse(reordered["actorImprintInstances"].begin(), reordered["actorImprintInstances"].end());
		SceneLoadResult second = SceneLoader::LoadCandidate(reordered, fixture.context, "memory://reordered.scene");
		json canonicalSecond;
		Check(second &&
			VerifyLoadedScene(*second.scene, ids) &&
			SceneWriter::SerializeScene(second.scene.get(), canonicalSecond) &&
			canonicalSecond.dump(4) == canonicalFirst.dump(4),
			"Reordering both Scene arrays produces the same loaded Scene and canonical save");

		if (first.scene) first.scene->Finalize();
		if (second.scene) second.scene->Finalize();
	}

	void TestGuidProvenanceAndLocatedFailures()
	{
		Fixture fixture;
		json saved;
		SceneIds ids;
		Check(BuildSerializedScene(fixture, saved, ids), "Provenance fixture serializes");
		const std::string assetPath = "memory://provenance.scene";
		auto ExpectFailure = [&](json invalid, SceneLoadErrorCode code,
			const std::string& path, const char* label)
		{
			SceneLoadResult load = SceneLoader::LoadCandidate(invalid, fixture.context, assetPath);
			Check(!load &&
				!load.scene &&
				load.error.code == code &&
				load.error.path == path &&
				load.error.assetPath == assetPath &&
				!load.error.message.empty(),
				label);
		};

		json rootMismatch = saved;
		json* firstInstance = &rootMismatch["actorImprintInstances"][0];
		firstInstance->at("rootActorGuid") = FindMapping(*firstInstance, 20)->at("actorGuid");
		ExpectFailure(rootMismatch, SceneLoadErrorCode::InstanceDeserializationFailed,
			"/actorImprintInstances/0/rootActorGuid",
			"Definition root LocalObjectID mismatch is rejected at rootActorGuid");

		json ordinaryCollision = saved;
		ordinaryCollision["actors"][0]["actorId"] =
			ordinaryCollision["actorImprintInstances"][0]["actorGuids"][0]["actorGuid"];
		ExpectFailure(ordinaryCollision, SceneLoadErrorCode::DuplicateActorGuid,
			"/actorImprintInstances/0/actorGuids/0/actorGuid",
			"Ordinary and Instance Actor GUID collision is rejected globally");

		json instanceCollision = saved;
		json* firstMember = FindMapping(instanceCollision["actorImprintInstances"][0], 20);
		json* secondMember = FindMapping(instanceCollision["actorImprintInstances"][1], 20);
		secondMember->at("actorGuid") = firstMember->at("actorGuid");
		std::size_t secondMemberIndex = 0;
		for (; secondMemberIndex < instanceCollision["actorImprintInstances"][1]["actorGuids"].size(); ++secondMemberIndex)
			if (instanceCollision["actorImprintInstances"][1]["actorGuids"][secondMemberIndex]["localObjectId"] == 20) break;
		ExpectFailure(instanceCollision, SceneLoadErrorCode::DuplicateActorGuid,
			"/actorImprintInstances/1/actorGuids/" + std::to_string(secondMemberIndex) + "/actorGuid",
			"Two Instance records cannot claim the same Actor GUID");

		json invalidParent = saved;
		invalidParent["actorImprintInstances"][0]["externalParentActorGuid"] =
			invalidParent["actorImprintInstances"][1]["rootActorGuid"];
		ExpectFailure(invalidParent, SceneLoadErrorCode::InvalidHierarchy,
			"/actorImprintInstances/0/externalParentActorGuid",
			"External parent provenance is restricted to ordinary Actors");

		json unknownInstanceField = saved;
		unknownInstanceField["actorImprintInstances"][0]["unexpected"] = true;
		ExpectFailure(unknownInstanceField, SceneLoadErrorCode::InstanceDeserializationFailed,
			"/actorImprintInstances/0",
			"Unknown Instance field is rejected at the Instance boundary");

		json missingAsset = saved;
		missingAsset["actorImprintInstances"][0]["assetGuid"] = GuidGenerator::Generate().ToString();
		ExpectFailure(missingAsset, SceneLoadErrorCode::InstanceDeserializationFailed,
			"/actorImprintInstances/0",
			"Missing ActorImprint asset fails with a located diagnostic");

		json migrated = saved;
		migrated["actorImprintInstances"][0]["sourceDefinitionRevision"] =
			DefinitionRevision::Generate().ToString();
		SceneLoadResult migration = SceneLoader::LoadCandidate(migrated, fixture.context, "memory://migration.scene");
		json normalized;
		Check(migration &&
			SceneWriter::SerializeScene(migration.scene.get(), normalized) &&
			normalized["actorImprintInstances"][0]["sourceDefinitionRevision"] ==
				saved["actorImprintInstances"][0]["sourceDefinitionRevision"],
			"Scene candidate applies revision migration and normalizes the next save");
		if (migration.scene) migration.scene->Finalize();
	}

	void TestWholeSceneFailureSuppressesLifecycleAndReleasesPins()
	{
		Fixture fixture;
		json saved;
		SceneIds ids;
		Check(BuildSerializedScene(fixture, saved, ids), "Failure fixture serializes");
		json* instance = FindInstance(saved, ids.rootA);
		json* probeOverride = instance ? FindOverride(*instance, 12) : nullptr;
		Check(probeOverride != nullptr, "Failure fixture contains the Probe override");
		if (!probeOverride) return;
		probeOverride->at("properties")["/target"] = {
			{ "type", "ActorReference" }, { "scope", "scene" },
			{ "actorGuid", GuidGenerator::Generate().ToString() }
		};

		SceneProbe::attachCount = 0;
		SceneProbe::destroyCount = 0;
		SceneLoadResult rejected = SceneLoader::LoadCandidate(saved, fixture.context, "memory://late-failure.scene");
		std::string expectedPath;
		for (std::size_t index = 0; index < saved["actorImprintInstances"].size(); ++index)
			if (saved["actorImprintInstances"][index]["rootActorGuid"] == ids.rootA.ToString())
				expectedPath = "/actorImprintInstances/" + std::to_string(index);

		Check(!rejected &&
			rejected.error.code == SceneLoadErrorCode::ReferenceResolutionFailed &&
			rejected.error.path == expectedPath &&
			rejected.error.assetPath == "memory://late-failure.scene" &&
			!rejected.error.message.empty(),
			"One invalid reference rejects the complete Scene candidate");
		Check(SceneProbe::attachCount == 0 &&
			SceneProbe::destroyCount == 0,
			"Failed unpublished candidate invokes neither OnAttach nor OnDestroy");
		Check(fixture.system.Unload(fixture.imprint),
			"Failed candidate releases every ActorImprint definition pin");
	}

	void TestUIIncompatibleInstanceFailsWithoutTransformConversion()
	{
		Fixture fixture;
		json saved;
		SceneIds ids;
		Check(BuildSerializedScene(fixture, saved, ids), "UI incompatibility fixture serializes");

		RectTransform rectTransform;
		Canvas canvas;
		json rectData;
		json canvasData;
		rectTransform.Serialize(rectData);
		canvas.Serialize(canvasData);
		for (json& actor : saved["actors"])
		{
			if (actor["actorId"] != ids.parent.ToString()) continue;
			actor["components"] = json::array({
				{ { "type", "RectTransform" }, { "data", rectData } },
				{ { "type", "Canvas" }, { "data", canvasData } },
			});
		}

		const std::string firstRoot = (std::min)(ids.rootA.ToString(), ids.rootB.ToString());
		std::string expectedPath;
		for (std::size_t index = 0; index < saved["actorImprintInstances"].size(); ++index)
		{
			if (saved["actorImprintInstances"][index]["rootActorGuid"] == firstRoot)
				expectedPath = "/actorImprintInstances/" + std::to_string(index);
		}

		SceneProbe::attachCount = 0;
		SceneProbe::destroyCount = 0;
		SceneLoadResult rejected = SceneLoader::LoadCandidate(
			saved, fixture.context, "memory://ui-incompatible.scene");
		Check(!rejected &&
			rejected.error.code == SceneLoadErrorCode::UIHierarchyFailed &&
			rejected.error.path == expectedPath,
			"UI-incompatible Instance structure rejects the whole Scene at its Instance path");
		Check(SceneProbe::attachCount == 0 &&
			SceneProbe::destroyCount == 0,
			"UI failure neither converts nor invokes lifecycle callbacks on Instance members");
		Check(fixture.system.Unload(fixture.imprint),
			"UI failure releases every candidate definition pin");
	}

	void TestSceneClonerPreservesInstances()
	{
		Fixture fixture;
		SceneBase source;
		SceneIds ids;
		Check(PopulateScene(fixture, source, ids), "SceneCloner source with Instances is created");
		std::unique_ptr<SceneBase> clone = SceneCloner::Clone(&source, fixture.context);
		json sourceJson;
		json cloneJson;
		Check(clone &&
			VerifyLoadedScene(*clone, ids) &&
			SceneWriter::SerializeScene(&source, sourceJson) &&
			SceneWriter::SerializeScene(clone.get(), cloneJson) &&
			sourceJson == cloneJson,
			"SceneCloner uses the v4 candidate contract and preserves Instance provenance");
		if (clone) clone->Finalize();
		source.Finalize();
	}
}

int main()
{
	RegisterProbe();
	TestVersion4RoundTripOrderAndDeterminism();
	TestGuidProvenanceAndLocatedFailures();
	TestWholeSceneFailureSuppressesLifecycleAndReleasesPins();
	TestUIIncompatibleInstanceFailsWithoutTransformConversion();
	TestSceneClonerPreservesInstances();
	ComponentRegistry::Get().UnregisterAllGameComponents();
	return failures ? 1 : 0;
}
