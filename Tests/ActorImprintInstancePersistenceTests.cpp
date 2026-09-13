#include "Engine/ActorImprint/ActorImprintInstanceDeserializer.h"
#include "Engine/ActorImprint/ActorImprintInstanceSerializer.h"
#include "Engine/ActorImprint/ActorImprintInstanceSnapshot.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorReference.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetReference.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace
{
	using json = nlohmann::json;
	int failures = 0;

	void Check(bool condition, const char* name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { ++failures; std::cerr << "[FAIL] " << name << '\n'; }
	}

	class OverrideProbe final : public Component
	{
	public:
		float weight = 1.0f;
		Vector3 offset = Vector3::Zero();
		ActorReference target;
		AssetReference<TextureAsset> texture;
		float low = 0.0f;
		float high = 10.0f;

		bool ResolveReferences(SceneBase& scene) override
		{
			return !target.HasValue() || target.Resolve(scene) != nullptr;
		}

	private:
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	void RegisterProbe()
	{
		TypeMetadataBuilder<OverrideProbe> builder("OverrideProbeET12");
		builder.Property("weight", &OverrideProbe::weight);
		builder.Property("offset", &OverrideProbe::offset);
		builder.Property("target", &OverrideProbe::target);
		builder.Property("texture", &OverrideProbe::texture);
		builder.Property("low", &OverrideProbe::low);
		builder.Property("high", &OverrideProbe::high);
		builder.SetValidator([](const OverrideProbe& probe) -> std::optional<ReflectionError>
		{
			if (probe.low <= probe.high) return std::nullopt;
			return ReflectionError{ ReflectionErrorCode::TypeInvariantViolation,
				PropertyPath::FromString("/high"), "OverrideProbe low must not exceed high." };
		});
		ComponentRegistry::Get().RegisterGameComponent("OverrideProbeET12",
			[]() -> Component* { return new OverrideProbe(); }, typeid(OverrideProbe),
			std::make_unique<TypeMetadata>(*builder.Build()));
	}

	json LocalReference(LocalObjectId id)
	{
		return { { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", id } };
	}

	json Definition(const Guid& textureGuid)
	{
		std::ifstream stream("Tests/Fixtures/ActorImprint/Minimal.imprint");
		json source = json::parse(stream);
		json child = source["actors"][0];
		child["localObjectId"] = 20;
		child["parentLocalObjectId"] = 10;
		child["properties"]["name"] = "Child";
		child["components"][0]["localObjectId"] = 21;
		source["actors"].push_back(std::move(child));
		source["actors"][0]["components"].push_back({
			{ "localObjectId", 12 },
			{ "type", "OverrideProbeET12" },
			{ "properties", {
				{ "weight", 1.0 },
				{ "offset", { 0.0, 0.0, 0.0 } },
				{ "target", LocalReference(20) },
				{ "texture", textureGuid.ToString() },
				{ "low", 0.0 },
				{ "high", 10.0 },
			} },
		});
		source["nextLocalObjectId"] = 30;
		return source;
	}

	struct Fixture
	{
		std::filesystem::path path = std::filesystem::temp_directory_path() /
			("101ImprintInstancePersistence-" + GuidGenerator::Generate().ToString());
		Guid assetGuid = GuidGenerator::Generate();
		Guid textureGuid = GuidGenerator::Generate();
		AssetManager assets;
		ActorImprintSystem system{ assets };
		EngineContext context{};
		SceneBase scene;
		ActorImprintHandle imprint;

		Fixture()
		{
			std::filesystem::create_directories(path);
			std::ofstream(path / "texture.png").put('x');
			MetaFile::Save((path / "texture.png").string(), textureGuid);
			std::ofstream(path / "test.imprint") << Definition(textureGuid);
			MetaFile::Save((path / "test.imprint").string(), assetGuid);
			Check(assets.Initialize(path.string(), nullptr, nullptr), "Persistence fixture catalog initializes");
			context.pAssetManager = &assets;
			context.pActorImprintSystem = &system;
			scene.Initialize(context);
			imprint = system.Load(assetGuid);
			Check(!imprint.IsNull(), "Persistence fixture definition loads");
		}

		~Fixture()
		{
			scene.Finalize();
			if (std::filesystem::absolute(path).parent_path() == std::filesystem::absolute(std::filesystem::temp_directory_path()))
				std::filesystem::remove_all(path);
		}
	};

	const json* FindOverride(const json& record, LocalObjectId target, const char* path)
	{
		const auto overrides = record.find("propertyOverrides");
		if (overrides == record.end()) return nullptr;
		for (const auto& entry : *overrides)
		{
			if (entry["targetLocalObjectId"] != target) continue;
			const auto property = entry["properties"].find(path);
			return property == entry["properties"].end() ? nullptr : &*property;
		}
		return nullptr;
	}

	void RoundTripAndAtomicity()
	{
		Fixture fixture;
		Actor* root = fixture.system.Instantiate(fixture.scene, fixture.imprint);
		Check(root != nullptr, "Persistence fixture materializes");
		if (!root) return;
		const auto rootHandle = root->GetHandle();
		auto& registry = fixture.scene.GetImprintInstances();
		auto* transform = static_cast<Transform*>(registry.ResolveComponent(rootHandle, 11));
		auto* probe = static_cast<OverrideProbe*>(registry.ResolveComponent(rootHandle, 12));
		Actor* child = registry.ResolveActor(rootHandle, 20);

		json unedited;
		Check(ActorImprintInstanceSerializer::Serialize(fixture.scene, rootHandle, unedited) &&
			!unedited.contains("propertyOverrides"), "Unedited Instance emits no Property Overrides");
		root->SetName("Elite Enemy");
		transform->SetLocalPosition({ 10.0f, 0.0f, 5.0f });
		probe->weight = 2.0f;
		probe->texture.Clear();

		json saved;
		ActorImprintInstanceSerializationError serializationError;
		Check(ActorImprintInstanceSerializer::Serialize(fixture.scene, rootHandle, saved, &serializationError),
			"Edited Instance serializes from live values");
		const json* name = FindOverride(saved, 10, "/name");
		const json* position = FindOverride(saved, 11, "/position");
		const json* weight = FindOverride(saved, 12, "/weight");
		const json* texture = FindOverride(saved, 12, "/texture");
		Check(name && *name == "Elite Enemy" && position && position->is_array() && position->size() == 3 &&
			weight && *weight == 2.0 && texture && texture->is_null(),
			"Actor, atomic array, float and explicit null Overrides use LocalObjectID paths");
		json repeated;
		Check(ActorImprintInstanceSerializer::Serialize(fixture.scene, rootHandle, repeated) && repeated == saved,
			"Instance Serializer output is deterministic");
		const json beforeInvalidLiveSave = repeated;
		probe->low = 20.0f;
		probe->high = 10.0f;
		Check(!ActorImprintInstanceSerializer::Serialize(fixture.scene, rootHandle, repeated, &serializationError) &&
			repeated == beforeInvalidLiveSave,
			"Serializer rejects a live cross-property invariant violation without changing its output");
		probe->low = 0.0f;
		probe->high = 10.0f;
		auto ExpectAtomicRestoreFailure = [&](const json& invalid, const char* label)
		{
			SceneBase destination;
			destination.Initialize(fixture.context);
			Actor* sentinel = destination.AddRootActor(ActorFactory::CreateEmptyActor({}));
			const std::size_t actorCount = destination.GetActorPool().Count();
			const ActorHandle sentinelHandle = sentinel ? sentinel->GetHandle() : ActorHandle{};
			const Guid sentinelGuid = sentinel ? sentinel->GetGuid() : Guid{};
			ActorImprintInstanceDeserializationError error;
			Actor* rejected = ActorImprintInstanceDeserializer::Restore(invalid, destination, fixture.system, &error);
			Check(sentinel && !rejected && destination.GetActorPool().Count() == actorCount &&
				destination.ResolveActor(sentinelHandle) == sentinel &&
				destination.FindActorHandle(sentinelGuid) == sentinelHandle &&
				destination.GetImprintInstances().GetInstances().empty(), label);
			destination.Finalize();
		};
		json missingExternalParent = saved;
		missingExternalParent["externalParentActorGuid"] = GuidGenerator::Generate().ToString();
		ExpectAtomicRestoreFailure(missingExternalParent,
			"Missing external parent is rejected without changing the destination Scene");
		SceneBase destroyingParentScene;
		destroyingParentScene.Initialize(fixture.context);
		Actor* destroyingParent = destroyingParentScene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		json destroyingExternalParent = saved;
		destroyingExternalParent["externalParentActorGuid"] = destroyingParent->GetGuid().ToString();
		Check(destroyingParentScene.RemoveActor(destroyingParent), "External-parent fixture enters pending destruction");
		const std::size_t destroyingParentCount = destroyingParentScene.GetActorPool().Count();
		ActorImprintInstanceDeserializationError parentError;
		Check(!ActorImprintInstanceDeserializer::Restore(destroyingExternalParent, destroyingParentScene,
			fixture.system, &parentError) && destroyingParentScene.GetActorPool().Count() == destroyingParentCount &&
			destroyingParentScene.GetImprintInstances().GetInstances().empty(),
			"Destroying external parent is rejected before Instance construction");
		destroyingParentScene.Finalize();
		json memberExternalParent = saved;
		memberExternalParent["externalParentActorGuid"] = child->GetGuid().ToString();
		ActorImprintPreparedRestore parentSentinel;
		parentSentinel.input.rootActorGuid = GuidGenerator::Generate();
		const Guid parentSentinelGuid = parentSentinel.input.rootActorGuid;
		const std::size_t fixtureActorCount = fixture.scene.GetActorPool().Count();
		const std::size_t fixtureInstanceCount = fixture.scene.GetImprintInstances().GetInstances().size();
		Check(!ActorImprintInstanceDeserializer::Deserialize(memberExternalParent, fixture.scene,
			fixture.system, parentSentinel, &parentError) && parentSentinel.input.rootActorGuid == parentSentinelGuid &&
			fixture.scene.GetActorPool().Count() == fixtureActorCount &&
			fixture.scene.GetImprintInstances().GetInstances().size() == fixtureInstanceCount,
			"ActorImprint member cannot be used as an external parent and Prepare output stays unchanged");

		SceneBase restoredScene;
		restoredScene.Initialize(fixture.context);
		ActorImprintInstanceDeserializationError restoreError;
		Actor* restored = ActorImprintInstanceDeserializer::Restore(saved, restoredScene, fixture.system, &restoreError);
		Check(restored && restored->GetGuid() == root->GetGuid() && restored->GetName() == "Elite Enemy",
			"Same-revision record restores saved identity and Actor Override");
		if (restored)
		{
			const auto restoredHandle = restored->GetHandle();
			auto* restoredTransform = static_cast<Transform*>(restoredScene.GetImprintInstances().ResolveComponent(restoredHandle, 11));
			auto* restoredProbe = static_cast<OverrideProbe*>(restoredScene.GetImprintInstances().ResolveComponent(restoredHandle, 12));
			Actor* restoredChild = restoredScene.GetImprintInstances().ResolveActor(restoredHandle, 20);
			const Vector3 restoredPosition = restoredTransform->GetLocalPosition();
			Check(restoredChild && restoredProbe && restoredProbe->target.Resolve(restoredScene) == restoredChild,
				"Same-Instance ActorReference restores through LocalObjectID");
			Check(restoredPosition.x == 10.0f && restoredPosition.y == 0.0f && restoredPosition.z == 5.0f &&
				restoredProbe->weight == 2.0f && !restoredProbe->texture.HasValue(),
				"Completed reflected objects retain array, float and null values");
		}
		restoredScene.Finalize();

		json invalidInvariant = saved;
		for (auto& target : invalidInvariant["propertyOverrides"])
			if (target["targetLocalObjectId"] == 12)
			{
				target["properties"]["/low"] = 20.0;
				target["properties"]["/high"] = 10.0;
			}
		SceneBase invalidScene;
		invalidScene.Initialize(fixture.context);
		Check(!ActorImprintInstanceDeserializer::Restore(invalidInvariant, invalidScene, fixture.system, &restoreError) &&
			invalidScene.GetActorPool().Count() == 0 && invalidScene.GetImprintInstances().GetInstances().empty(),
			"Completed-object invariant failure leaves no partial Instance");
		invalidScene.Finalize();
		json sameMissingPath = saved;
		for (auto& target : sameMissingPath["propertyOverrides"])
			if (target["targetLocalObjectId"] == 11) target["properties"]["/removed"] = true;
		ExpectAtomicRestoreFailure(sameMissingPath,
			"Same-revision missing Override path fails without changing the destination Scene");
		json sameMissingTarget = saved;
		sameMissingTarget["propertyOverrides"].push_back({
			{ "targetLocalObjectId", 999 }, { "properties", { { "/name", "missing" } } }
		});
		ExpectAtomicRestoreFailure(sameMissingTarget,
			"Same-revision missing Override target fails without changing the destination Scene");
		json sameWrongShape = saved;
		for (auto& target : sameWrongShape["propertyOverrides"])
			if (target["targetLocalObjectId"] == 11) target["properties"]["/position"] = "wrong shape";
		ExpectAtomicRestoreFailure(sameWrongShape,
			"Same-revision incompatible Override representation is fatal and atomic");

		json internalAsScene = saved;
		for (auto& target : internalAsScene["propertyOverrides"])
			if (target["targetLocalObjectId"] == 12)
				target["properties"]["/target"] = {
					{ "type", "ActorReference" }, { "scope", "scene" },
					{ "actorGuid", child->GetGuid().ToString() }
				};
		SceneBase internalScopeScene;
		internalScopeScene.Initialize(fixture.context);
		Check(!ActorImprintInstanceDeserializer::Restore(internalAsScene, internalScopeScene,
			fixture.system, &restoreError) && internalScopeScene.GetActorPool().Count() == 0,
			"Internal ActorReference cannot bypass LocalObjectID persistence with Scene scope");
		internalScopeScene.Finalize();
		json danglingLocal = saved;
		danglingLocal["sourceDefinitionRevision"] = DefinitionRevision::Generate().ToString();
		for (auto& target : danglingLocal["propertyOverrides"])
			if (target["targetLocalObjectId"] == 12)
				target["properties"]["/target"] = LocalReference(999);
		ExpectAtomicRestoreFailure(danglingLocal,
			"Different-revision compatible but dangling local ActorReference remains fatal");
		json missingAsset = saved;
		missingAsset["sourceDefinitionRevision"] = DefinitionRevision::Generate().ToString();
		for (auto& target : missingAsset["propertyOverrides"])
			if (target["targetLocalObjectId"] == 12)
				target["properties"]["/texture"] = GuidGenerator::Generate().ToString();
		ExpectAtomicRestoreFailure(missingAsset,
			"Different-revision missing Asset reference fails after staging without partial publication");
		json wrongAssetType = saved;
		wrongAssetType["sourceDefinitionRevision"] = DefinitionRevision::Generate().ToString();
		for (auto& target : wrongAssetType["propertyOverrides"])
			if (target["targetLocalObjectId"] == 12)
				target["properties"]["/texture"] = fixture.assetGuid.ToString();
		ExpectAtomicRestoreFailure(wrongAssetType,
			"Different-revision Asset type mismatch fails after staging without partial publication");
		json embeddedNullAsset = saved;
		for (auto& target : embeddedNullAsset["propertyOverrides"])
			if (target["targetLocalObjectId"] == 12)
				target["properties"]["/texture"] =
					fixture.textureGuid.ToString() + std::string("\0suffix", 7);
		ExpectAtomicRestoreFailure(embeddedNullAsset,
			"Malformed compatible Asset GUID remains fatal instead of being truncated");

		json migrated = saved;
		migrated["sourceDefinitionRevision"] = DefinitionRevision::Generate().ToString();
		const Guid retiredGuid = GuidGenerator::Generate();
		migrated["actorGuids"] = json::array({
			{ { "localObjectId", 10 }, { "actorGuid", root->GetGuid().ToString() } },
			{ { "localObjectId", 999 }, { "actorGuid", retiredGuid.ToString() } },
		});
		migrated["propertyOverrides"] = json::array({
			{ { "targetLocalObjectId", 10 }, { "properties", { { "/name", "Migrated" } } } },
			{ { "targetLocalObjectId", 11 }, { "properties", { { "/position", "old shape" }, { "/removed", true } } } },
			{ { "targetLocalObjectId", 12 }, { "properties", { { "/texture", nullptr } } } },
			{ { "targetLocalObjectId", 999 }, { "properties", { { "/removed", true } } } },
		});
		SceneBase invalidStructureScene;
		invalidStructureScene.Initialize(fixture.context);
		ActorImprintPreparedRestore invalidStructure;
		Check(ActorImprintInstanceDeserializer::Deserialize(migrated, invalidStructureScene,
			fixture.system, invalidStructure, &restoreError), "Valid migration record prepares before direct-input validation test");
		for (auto& target : invalidStructure.input.propertyOverrides)
			if (target.targetLocalObjectId == 999)
				target.properties.emplace_back(*PropertyPath::FromString("/removed/child"), true);
		ActorImprintMaterializationError materializationError;
		Check(!fixture.system.RestoreInstance(invalidStructureScene, invalidStructure.imprint,
			invalidStructure.input, &materializationError) && invalidStructureScene.GetActorPool().Count() == 0 &&
			invalidStructureScene.GetImprintInstances().GetInstances().empty(),
			"Deleted Override target still rejects ancestor path conflicts before commit");
		invalidStructureScene.Finalize();
		SceneBase invalidValueScene;
		invalidValueScene.Initialize(fixture.context);
		ActorImprintPreparedRestore invalidValue;
		Check(ActorImprintInstanceDeserializer::Deserialize(migrated, invalidValueScene,
			fixture.system, invalidValue, &restoreError), "Valid migration record prepares before direct value validation test");
		for (auto& target : invalidValue.input.propertyOverrides)
			if (target.targetLocalObjectId == 999)
				target.properties[0].value = (std::numeric_limits<double>::quiet_NaN)();
		Check(!fixture.system.RestoreInstance(invalidValueScene, invalidValue.imprint,
			invalidValue.input, &materializationError) && invalidValueScene.GetActorPool().Count() == 0,
			"Deleted Override target still rejects non-finite JSON before stale classification");
		invalidValueScene.Finalize();
		json preservedMigration = saved;
		preservedMigration["sourceDefinitionRevision"] = DefinitionRevision::Generate().ToString();
		SceneBase preservedScene;
		preservedScene.Initialize(fixture.context);
		Actor* preservedRoot = ActorImprintInstanceDeserializer::Restore(
			preservedMigration, preservedScene, fixture.system, &restoreError);
		Actor* preservedChild = preservedRoot
			? preservedScene.GetImprintInstances().ResolveActor(preservedRoot->GetHandle(), 20) : nullptr;
		Check(preservedRoot && preservedRoot->GetGuid() == root->GetGuid() && preservedChild &&
			preservedChild->GetGuid() == child->GetGuid(),
			"Revision migration preserves GUIDs for every surviving Actor LocalObjectID");
		preservedScene.Finalize();
		SceneBase rootMismatchScene;
		rootMismatchScene.Initialize(fixture.context);
		Actor* rootMismatchSentinel = rootMismatchScene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		ActorImprintPreparedRestore rootMismatch;
		Check(ActorImprintInstanceDeserializer::Deserialize(preservedMigration, rootMismatchScene,
			fixture.system, rootMismatch, &restoreError), "Valid migration prepares before root mapping validation test");
		rootMismatch.input.actorGuids[10] = GuidGenerator::Generate();
		const std::size_t rootMismatchCount = rootMismatchScene.GetActorPool().Count();
		Check(!fixture.system.RestoreInstance(rootMismatchScene, rootMismatch.imprint, rootMismatch.input,
			&materializationError) && rootMismatchScene.GetActorPool().Count() == rootMismatchCount &&
			rootMismatchSentinel && rootMismatchScene.ResolveActor(rootMismatchSentinel->GetHandle()) == rootMismatchSentinel &&
			rootMismatchScene.GetImprintInstances().GetInstances().empty(),
			"Current root LocalObjectID mapping mismatch is fatal before Scene publication");
		rootMismatchScene.Finalize();
		SceneBase migratedScene;
		migratedScene.Initialize(fixture.context);
		Actor* migratedRoot = ActorImprintInstanceDeserializer::Restore(migrated, migratedScene, fixture.system, &restoreError);
		Check(migratedRoot && migratedRoot->GetName() == "Migrated", "Revision migration retains valid Overrides");
		if (migratedRoot)
		{
			const auto migratedHandle = migratedRoot->GetHandle();
			Actor* migratedChild = migratedScene.GetImprintInstances().ResolveActor(migratedHandle, 20);
			auto* migratedTransform = static_cast<Transform*>(migratedScene.GetImprintInstances().ResolveComponent(migratedHandle, 11));
			Check(migratedChild && migratedChild->GetGuid() != retiredGuid &&
				migratedTransform->GetLocalPosition().x == 0.0f,
				"Revision migration generates added Actor identity and drops deleted mapping and stale values");
			json normalized;
			Check(ActorImprintInstanceSerializer::Serialize(migratedScene, migratedHandle, normalized) &&
				normalized["sourceDefinitionRevision"] == fixture.system.Resolve(fixture.imprint)->GetRevision().ToString() &&
				normalized.dump().find("removed") == std::string::npos &&
				normalized.dump().find("old shape") == std::string::npos,
				"Next save rewrites migration against the current revision without stale entries");
		}
		migratedScene.Finalize();

		Actor* parent = fixture.scene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		Check(parent && fixture.scene.ReparentActor(root, parent), "Snapshot fixture gives the Instance an ordinary parent");
		ActorImprintInstanceSnapshot snapshot;
		Check(snapshot.Capture(fixture.scene, rootHandle), "Instance snapshot captures persistence identity and Overrides");
		const Guid rootGuid = root->GetGuid();
		const Guid childGuid = child->GetGuid();
		Check(fixture.system.DestroyInstance(fixture.scene, rootHandle), "Snapshot fixture destroys the whole Instance");
		Check(!ActorImprintInstanceSerializer::Serialize(fixture.scene, rootHandle, repeated, &serializationError),
			"Destroying Instance cannot be serialized");
		fixture.scene.EditorUpdate(0.0f);
		Actor* undo = snapshot.Restore(fixture.scene, fixture.system, &restoreError);
		Check(undo && undo->GetGuid() == rootGuid && undo->GetParent() == parent,
			"Snapshot restore uses RestoreInstance and reconnects the external parent by GUID");
		if (undo)
		{
			auto* undoProbe = static_cast<OverrideProbe*>(fixture.scene.GetImprintInstances().ResolveComponent(undo->GetHandle(), 12));
			auto* undoTransform = static_cast<Transform*>(fixture.scene.GetImprintInstances().ResolveComponent(undo->GetHandle(), 11));
			Actor* undoChild = fixture.scene.GetImprintInstances().ResolveActor(undo->GetHandle(), 20);
			Check(undo->GetName() == "Elite Enemy" && undoProbe && undoProbe->weight == 2.0f &&
				!undoProbe->texture.HasValue(), "Snapshot restore retains live Property Overrides");
			Check(undoChild && undoChild->GetGuid() == childGuid && undoProbe &&
				undoProbe->target.Resolve(fixture.scene) == undoChild && undoTransform &&
				undoTransform->GetLocalPosition().x == 10.0f && undoTransform->GetLocalPosition().z == 5.0f,
				"Snapshot restores child identity, local reference and Transform state");
		}

		Actor* external = fixture.scene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		if (undo && external)
		{
			auto* undoProbe = static_cast<OverrideProbe*>(fixture.scene.GetImprintInstances().ResolveComponent(undo->GetHandle(), 12));
			undoProbe->target.Set(external);
			json externalRecord;
			Check(ActorImprintInstanceSerializer::Serialize(fixture.scene, undo->GetHandle(), externalRecord) &&
				FindOverride(externalRecord, 12, "/target") &&
				*FindOverride(externalRecord, 12, "/target") == json{
					{ "type", "ActorReference" }, { "scope", "scene" }, { "actorGuid", external->GetGuid().ToString() } },
				"External ActorReference serializes with typed Scene scope");
			externalRecord["externalParentActorGuid"] = nullptr;

			SceneBase externalScene;
			externalScene.Initialize(fixture.context);
			Actor* externalTarget = externalScene.AddRootActor(
				ActorFactory::RestoreEmptyActor({}, external->GetGuid()));
			Actor* externalRestore = ActorImprintInstanceDeserializer::Restore(
				externalRecord, externalScene, fixture.system, &restoreError);
			auto* externalProbe = externalRestore ? static_cast<OverrideProbe*>(
				externalScene.GetImprintInstances().ResolveComponent(externalRestore->GetHandle(), 12)) : nullptr;
			Check(externalTarget && externalRestore && externalProbe &&
				externalProbe->target.Resolve(externalScene) == externalTarget &&
				externalProbe->target.Resolve(externalScene) == externalTarget,
				"External ActorReference resolves against the mutation-blocked destination before commit and after publication");
			externalScene.Finalize();

			SceneBase crossInstanceScene;
			crossInstanceScene.Initialize(fixture.context);
			Actor* otherInstance = fixture.system.Instantiate(crossInstanceScene, fixture.imprint);
			Actor* otherInstanceChild = otherInstance ? crossInstanceScene.GetImprintInstances().ResolveActor(
				otherInstance->GetHandle(), 20) : nullptr;
			json crossInstanceRecord = externalRecord;
			if (otherInstanceChild)
				for (auto& target : crossInstanceRecord["propertyOverrides"])
					if (target["targetLocalObjectId"] == 12)
						target["properties"]["/target"]["actorGuid"] = otherInstanceChild->GetGuid().ToString();
			Actor* crossInstanceRestore = otherInstanceChild ? ActorImprintInstanceDeserializer::Restore(
				crossInstanceRecord, crossInstanceScene, fixture.system, &restoreError) : nullptr;
			auto* crossInstanceProbe = crossInstanceRestore ? static_cast<OverrideProbe*>(
				crossInstanceScene.GetImprintInstances().ResolveComponent(crossInstanceRestore->GetHandle(), 12)) : nullptr;
			Check(otherInstanceChild && crossInstanceRestore && crossInstanceProbe &&
				crossInstanceProbe->target.Resolve(crossInstanceScene) == otherInstanceChild &&
				crossInstanceScene.GetImprintInstances().GetInstances().size() == 2,
				"Scene-scope ActorReference may target an Actor in another live Instance");
			crossInstanceScene.Finalize();

			json missingExternalReference = externalRecord;
			for (auto& target : missingExternalReference["propertyOverrides"])
				if (target["targetLocalObjectId"] == 12)
					target["properties"]["/target"]["actorGuid"] = GuidGenerator::Generate().ToString();
			SceneBase missingReferenceScene;
			missingReferenceScene.Initialize(fixture.context);
			Check(!ActorImprintInstanceDeserializer::Restore(missingExternalReference, missingReferenceScene,
				fixture.system, &restoreError) && missingReferenceScene.GetActorPool().Count() == 0 &&
				missingReferenceScene.GetImprintInstances().GetInstances().empty(),
				"Missing external ActorReference fails without publishing candidate Actors");
			missingReferenceScene.Finalize();

			SceneBase destroyingReferenceScene;
			destroyingReferenceScene.Initialize(fixture.context);
			Actor* destroyingTarget = destroyingReferenceScene.AddRootActor(
				ActorFactory::RestoreEmptyActor({}, external->GetGuid()));
			Check(destroyingTarget && destroyingReferenceScene.RemoveActor(destroyingTarget),
				"External-reference fixture enters pending destruction");
			const std::size_t destroyingReferenceCount = destroyingReferenceScene.GetActorPool().Count();
			Check(!ActorImprintInstanceDeserializer::Restore(externalRecord, destroyingReferenceScene,
				fixture.system, &restoreError) &&
				destroyingReferenceScene.GetActorPool().Count() == destroyingReferenceCount &&
				destroyingReferenceScene.GetImprintInstances().GetInstances().empty(),
				"Destroying external ActorReference target fails without publishing candidate Actors");
			destroyingReferenceScene.Finalize();
		}
	}
}

int main()
{
	RegisterProbe();
	RoundTripAndAtomicity();
	ComponentRegistry::Get().UnregisterAllGameComponents();
	return failures ? 1 : 0;
}
