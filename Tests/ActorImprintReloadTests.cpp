#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorReference.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/ActorImprint/ActorImprintInstanceRegistry.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Component.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneWriter.h"
#include "nlohmann/json.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace
{
	using json = nlohmann::json;
	constexpr LocalObjectId RootId = 10;
	constexpr LocalObjectId RemovedChildId = 20;
	constexpr LocalObjectId AddedChildId = 30;
	constexpr const char* AssetPath = "reload.imprint";
	int failures = 0;

	void Check(bool condition, const char* name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { ++failures; std::cerr << "[FAIL] " << name << '\n'; }
	}

	void ReportUnexpectedReload(const ActorImprintReloadResult& result,
		ActorImprintReloadStatus expected)
	{
		if (result.status == expected) return;
		std::cerr << "[RELOAD] status=" << static_cast<int>(result.status)
			<< std::endl;
	}

	class ReloadReferenceProbe final : public Component
	{
	public:
		ActorReference target;
		static inline int attachments = 0;

		bool ResolveReferences(SceneBase& scene) override
		{
			return !target.HasValue() || target.Resolve(scene) != nullptr;
		}

	private:
		void OnAttachOverride() override { ++attachments; }
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	void RegisterProbe()
	{
		TypeMetadataBuilder<ReloadReferenceProbe> builder("ActorImprintReloadReferenceProbeET14");
		builder.Property("target", &ReloadReferenceProbe::target);
		ComponentRegistry::Get().RegisterGameComponent(
			"ActorImprintReloadReferenceProbeET14",
			[]() -> Component* { return new ReloadReferenceProbe(); },
			typeid(ReloadReferenceProbe),
			std::make_unique<TypeMetadata>(*builder.Build()));
	}

	json MinimalDefinition()
	{
		std::ifstream stream("Tests/Fixtures/ActorImprint/Minimal.imprint");
		return json::parse(stream);
	}

	json InitialDefinition()
	{
		json definition = MinimalDefinition();
		definition["actors"][0]["properties"]["name"] = "OldRootDefault";

		json removedChild = definition["actors"][0];
		removedChild["localObjectId"] = RemovedChildId;
		removedChild["parentLocalObjectId"] = RootId;
		removedChild["properties"]["name"] = "RemovedChild";
		removedChild["components"][0]["localObjectId"] = RemovedChildId + 1;
		definition["actors"].push_back(std::move(removedChild));
		definition["nextLocalObjectId"] = RemovedChildId + 2;
		return definition;
	}

	json UpdatedDefinition(const DefinitionRevision& revision)
	{
		json definition = MinimalDefinition();
		definition["definitionRevision"] = revision.ToString();
		definition["actors"][0]["properties"]["name"] = "NewRootDefault";
		definition["actors"][0]["properties"]["is_active"] = false;

		json addedChild = definition["actors"][0];
		addedChild["localObjectId"] = AddedChildId;
		addedChild["parentLocalObjectId"] = RootId;
		addedChild["properties"]["name"] = "AddedChildDefault";
		addedChild["properties"]["is_active"] = true;
		addedChild["components"][0]["localObjectId"] = AddedChildId + 1;
		definition["actors"].push_back(std::move(addedChild));
		definition["nextLocalObjectId"] = AddedChildId + 2;
		return definition;
	}

	struct Fixture
	{
		std::filesystem::path root = std::filesystem::temp_directory_path() /
			("101ActorImprintReload-" + GuidGenerator::Generate().ToString());
		Guid assetGuid = GuidGenerator::Generate();
		AssetManager assets;
		ActorImprintSystem system{ assets };
		EngineContext context{};
		ActorImprintHandle imprint;

		Fixture()
		{
			std::filesystem::create_directories(root);
			Write(InitialDefinition());
			Check(MetaFile::Save((root / AssetPath).string(), assetGuid), "Reload fixture metadata preserves the Asset GUID");
			Check(assets.Initialize(root.string(), nullptr, nullptr), "Reload fixture catalog initializes");
			assets.TakePendingChanges();
			context.pAssetManager = &assets;
			context.pActorImprintSystem = &system;

			imprint = system.Load(assetGuid);
			if (imprint.IsNull())
				std::cerr << "Operation failed\n";
			Check(!imprint.IsNull(), "Reload fixture definition loads");
		}

		~Fixture()
		{
			if (std::filesystem::absolute(root).lexically_normal().parent_path() ==
				std::filesystem::absolute(std::filesystem::temp_directory_path()).lexically_normal())
				std::filesystem::remove_all(root);
		}

		void Write(const json& definition)
		{
			std::ofstream(root / AssetPath, std::ios::trunc) << definition;
		}

		void WriteBroken()
		{
			std::ofstream(root / AssetPath, std::ios::trunc) << "{";
		}

		AssetChange ObserveChange(AssetChangeKind expectedKind)
		{
			Check(assets.NotifyAssetChanged(AssetPath), "Catalog accepts the explicit ActorImprint change");
			const auto changes = assets.TakePendingChanges();
			for (const AssetChange& change : changes)
			{
				if (change.guid == assetGuid)
				{
					Check(change.kind == expectedKind &&
						change.type == AssetType::ActorImprint &&
						change.relativePath == AssetPath,
						"Catalog emits the expected same-GUID ActorImprint change");
					return change;
				}
			}
			Check(false, "Catalog emits an ActorImprint change for the fixture Asset");
			return { expectedKind, AssetType::ActorImprint, assetGuid, AssetPath };
		}
	};

	struct SceneState
	{
		Guid rootGuid;
		Guid removedChildGuid;
		Guid observerGuid;
		Actor* originalRoot = nullptr;
		Actor* originalRemovedChild = nullptr;
	};

	bool AddReferenceProbe(SceneBase& scene, SceneState& state, Actor* target)
	{
		auto observerOwned = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "DeletionObserver"));
		auto* probe = observerOwned
			? observerOwned->AddComponent<ReloadReferenceProbe>()
			: nullptr;
		if (probe && target && !probe->target.Set(target)) return false;
		Actor* observer = scene.AddRootActor(std::move(observerOwned));
		if (!observer || !probe) return false;
		state.observerGuid = observer->GetGuid();
		return true;
	}

	std::unique_ptr<SceneBase> MakeScene(Fixture& fixture, const std::string& overrideName,
		SceneState& state, bool addReferenceToRemovedChild = false, bool addMainCamera = false)
	{
		auto scene = std::make_unique<SceneBase>();
		scene->Initialize(fixture.context);
		Actor* root = fixture.system.Instantiate(*scene, fixture.imprint);
		Actor* removedChild = root
			? scene->GetImprintInstances().ResolveActor(root->GetHandle(), RemovedChildId)
			: nullptr;
		Check(root && removedChild, "Reload fixture materializes the initial hierarchy");
		if (!root || !removedChild) return scene;

		root->SetName(overrideName);
		state.rootGuid = root->GetGuid();
		state.removedChildGuid = removedChild->GetGuid();
		state.originalRoot = root;
		state.originalRemovedChild = removedChild;

		if (addReferenceToRemovedChild)
		{
			Check(AddReferenceProbe(*scene, state, removedChild), "Rollback fixture creates an ordinary Actor reference to the removed member");
		}
		if (addMainCamera)
		{
			auto cameraOwned = ActorFactory::CreateActor(
				ActorType::Camera,
				Actor::InitDesc(true, ActorTags::MainCamera, "MainCamera"));
			Check(scene->AddRootActor(std::move(cameraOwned)) != nullptr, "Missing-state fixture has a valid MainCamera for public-save checks");
		}

		return scene;
	}

	std::span<std::unique_ptr<SceneBase>* const> SceneSpan(
		std::unique_ptr<SceneBase>** owners, std::size_t count)
	{
		return { owners, count };
	}

	bool VerifyUpdatedScene(SceneBase& scene, const SceneState& before,
		const std::string& expectedOverride, const DefinitionRevision& revision)
	{
		Actor* root = scene.ResolveActor(before.rootGuid);
		if (!root) return false;
		const ActorImprintInstanceRecord* record =
			scene.GetImprintInstances().FindInstance(root->GetHandle());
		Actor* addedChild = scene.GetImprintInstances().ResolveActor(root->GetHandle(), AddedChildId);
		return record && record->assetGuid.IsValid() && record->sourceRevision == revision &&
			record->actors.contains(RootId) && record->actors.contains(AddedChildId) &&
			!record->actors.contains(RemovedChildId) &&
			root->GetName() == expectedOverride && !root->IsActive() &&
			addedChild && addedChild->GetName() == "AddedChildDefault" && addedChild->IsActive() &&
			addedChild->GetParent() == root && addedChild->GetGuid().IsValid() &&
			addedChild->GetGuid() != before.removedChildGuid &&
			!scene.ResolveActor(before.removedChildGuid);
	}

	void TestMultipleSceneAtomicSuccess()
	{
		Fixture fixture;
		const ActorImprint* oldDefinition = fixture.system.Resolve(fixture.imprint);
		if (!oldDefinition) return;
		const DefinitionRevision oldRevision = oldDefinition->GetRevision();
		const DefinitionRevision newRevision = DefinitionRevision::Generate();

		SceneState firstState;
		SceneState secondState;
		auto first = MakeScene(fixture, "FirstSceneOverride", firstState);
		auto second = MakeScene(fixture, "SecondSceneOverride", secondState);
		if (!firstState.originalRoot || !secondState.originalRoot) return;
		SceneBase* firstOwnerBefore = first.get();
		SceneBase* secondOwnerBefore = second.get();

		fixture.Write(UpdatedDefinition(newRevision));
		const AssetChange change = fixture.ObserveChange(AssetChangeKind::Modified);
		std::array<std::unique_ptr<SceneBase>*, 2> owners{ &first, &second };
		ActorImprintReloadResult result = fixture.system.Reload(
			change, SceneSpan(owners.data(), owners.size()));
		ReportUnexpectedReload(result, ActorImprintReloadStatus::Reloaded);

		Check(result.status == ActorImprintReloadStatus::Reloaded &&
			result.previousRevision == oldRevision &&
			result.currentRevision == newRevision &&
			result.affectedSceneIndices == std::vector<std::size_t>{ 0, 1 },
			"Two live Scenes reload in one successful transaction");
		const ActorImprint* newDefinition = fixture.system.Resolve(fixture.imprint);
		Check(first.get() != firstOwnerBefore &&
			second.get() != secondOwnerBefore &&
			newDefinition &&
			newDefinition != oldDefinition &&
			newDefinition->GetRevision() == newRevision,
			"Successful commit swaps both Scene owners and the definition together");
		Check(VerifyUpdatedScene(*first, firstState, "FirstSceneOverride", newRevision),
			"First Scene retains GUID and override while applying add, remove, and new defaults");
		Check(VerifyUpdatedScene(*second, secondState, "SecondSceneOverride", newRevision),
			"Second Scene retains GUID and override while applying add, remove, and new defaults");

		result.FinalizeRetiredScenes();
		result.FinalizeRetiredScenes();
	}

	void TestSceneCandidateFailureRollsBackEverything()
	{
		Fixture fixture;
		const ActorImprint* oldDefinition = fixture.system.Resolve(fixture.imprint);
		if (!oldDefinition) return;
		const DefinitionRevision oldRevision = oldDefinition->GetRevision();
		const DefinitionRevision newRevision = DefinitionRevision::Generate();

		SceneState firstState;
		SceneState secondState;
		auto first = MakeScene(fixture, "FirstRollbackOverride", firstState);
		auto second = MakeScene(fixture, "SecondRollbackOverride", secondState, true);
		Check(first && AddReferenceProbe(*first, firstState, nullptr), "Rollback fixture creates a valid probe in the first Scene");
		if (!firstState.originalRoot || !secondState.originalRoot || !secondState.observerGuid.IsValid()) return;
		SceneBase* firstOwnerBefore = first.get();
		SceneBase* secondOwnerBefore = second.get();
		const int attachmentsBeforeReload = ReloadReferenceProbe::attachments;

		fixture.Write(UpdatedDefinition(newRevision));
		const AssetChange change = fixture.ObserveChange(AssetChangeKind::Modified);
		std::array<std::unique_ptr<SceneBase>*, 2> owners{ &first, &second };
		ActorImprintReloadResult result = fixture.system.Reload(
			change, SceneSpan(owners.data(), owners.size()));
		ReportUnexpectedReload(result, ActorImprintReloadStatus::Failed);

		Check(result.status == ActorImprintReloadStatus::Failed,
			"A late failure in the second Scene rejects the reload with a located diagnostic");
		Check(first.get() == firstOwnerBefore &&
			second.get() == secondOwnerBefore &&
			fixture.system.Resolve(fixture.imprint) == oldDefinition &&
			fixture.system.Resolve(fixture.imprint)->GetRevision() == oldRevision,
			"Failed candidate construction publishes neither Scene nor definition");
		Check(ReloadReferenceProbe::attachments == attachmentsBeforeReload, "No Scene candidate attaches before every live Scene has validated");
		Check(first->ResolveActor(firstState.rootGuid) == firstState.originalRoot &&
			first->ResolveActor(firstState.removedChildGuid) == firstState.originalRemovedChild &&
			second->ResolveActor(secondState.rootGuid) == secondState.originalRoot &&
			second->ResolveActor(secondState.removedChildGuid) == secondState.originalRemovedChild,
			"Complete rollback preserves both original Actor graphs and GUID lookups");

		Actor* observer = second->ResolveActor(secondState.observerGuid);
		auto* probe = observer ? observer->GetComponentByClass<ReloadReferenceProbe>() : nullptr;
		const ActorImprintInstanceRecord* firstRecord =
			first->GetImprintInstances().FindInstance(firstState.originalRoot->GetHandle());
		const ActorImprintInstanceRecord* secondRecord =
			second->GetImprintInstances().FindInstance(secondState.originalRoot->GetHandle());
		Check(probe &&
			probe->target.Resolve(*second) == secondState.originalRemovedChild &&
			firstRecord &&
			firstRecord->sourceRevision == oldRevision &&
			secondRecord &&
			secondRecord->sourceRevision == oldRevision,
			"Rollback preserves ordinary references, registries, and source revisions");

		first.reset();
		second.reset();
		Check(fixture.system.Unload(fixture.imprint), "Rejected prepared Scenes release every temporary definition pin");
	}

	void TestIncompleteLiveSceneSet()
	{
		Fixture fixture;
		const ActorImprint* oldDefinition = fixture.system.Resolve(fixture.imprint);
		if (!oldDefinition) return;
		SceneState firstState;
		SceneState secondState;
		auto first = MakeScene(fixture, "FirstIncompleteOverride", firstState);
		auto second = MakeScene(fixture, "SecondIncompleteOverride", secondState);
		if (!firstState.originalRoot || !secondState.originalRoot) return;
		SceneBase* firstOwnerBefore = first.get();
		SceneBase* secondOwnerBefore = second.get();

		fixture.Write(UpdatedDefinition(DefinitionRevision::Generate()));
		const AssetChange change = fixture.ObserveChange(AssetChangeKind::Modified);
		std::array<std::unique_ptr<SceneBase>*, 1> incompleteOwners{ &first };
		ActorImprintReloadResult result = fixture.system.Reload(
			change, SceneSpan(incompleteOwners.data(), incompleteOwners.size()));

		Check(result.status == ActorImprintReloadStatus::Failed, "Reload rejects a Scene list that omits one live Instance owner");
		Check(first.get() == firstOwnerBefore &&
			second.get() == secondOwnerBefore &&
			fixture.system.Resolve(fixture.imprint) == oldDefinition &&
			fixture.system.GetAvailability(fixture.imprint) == ActorImprintAvailability::Available &&
			first->ResolveActor(firstState.removedChildGuid) == firstState.originalRemovedChild &&
			second->ResolveActor(secondState.removedChildGuid) == secondState.originalRemovedChild,
			"Incomplete Scene-set rejection leaves all live owners and the definition unchanged");
	}

	void TestRemovedMissingAndSameGuidRestore()
	{
		Fixture fixture;
		const ActorImprint* oldDefinition = fixture.system.Resolve(fixture.imprint);
		if (!oldDefinition) return;
		const DefinitionRevision oldRevision = oldDefinition->GetRevision();
		SceneState state;
		auto scene = MakeScene(fixture, "MissingAssetOverride", state, false, true);
		if (!state.originalRoot) return;
		SceneBase* ownerBefore = scene.get();
		std::array<std::unique_ptr<SceneBase>*, 1> owners{ &scene };

		Check(std::filesystem::remove(fixture.root / AssetPath), "Removal fixture deletes only the ActorImprint source file");
		const AssetChange removed = fixture.ObserveChange(AssetChangeKind::Removed);
		ActorImprintReloadResult missing = fixture.system.Reload(
			removed, SceneSpan(owners.data(), owners.size()));
		Check(missing.status == ActorImprintReloadStatus::Missing &&
			missing.previousRevision == oldRevision &&
			missing.currentRevision == oldRevision &&
			fixture.system.GetAvailability(fixture.imprint) == ActorImprintAvailability::Missing &&
			scene.get() == ownerBefore &&
			fixture.system.Resolve(fixture.imprint) == oldDefinition,
			"Removed Asset keeps the old definition and live Scene while entering Missing state");

		Check(fixture.system.Load(fixture.assetGuid).IsNull() &&
			!fixture.system.Instantiate(*scene, fixture.imprint, {}),
			"Removed catalog identity rejects new loads and Instance creation");
		json missingSave;
		Check(!SceneWriter::SerializeScene(scene.get(), missingSave), "Public Scene save rejects a live Instance whose Asset is Missing");

		fixture.WriteBroken();
		const AssetChange brokenRestore = fixture.ObserveChange(AssetChangeKind::Added);
		ActorImprintReloadResult rejected = fixture.system.Reload(
			brokenRestore, SceneSpan(owners.data(), owners.size()));
		Check(brokenRestore.guid == fixture.assetGuid &&
			rejected.status == ActorImprintReloadStatus::Failed &&
			fixture.system.GetAvailability(fixture.imprint) == ActorImprintAvailability::Missing &&
			scene.get() == ownerBefore &&
			fixture.system.Resolve(fixture.imprint) == oldDefinition &&
			scene->ResolveActor(state.removedChildGuid) == state.originalRemovedChild,
			"Broken same-GUID restoration is rejected without leaving Missing state");

		const DefinitionRevision restoredRevision = DefinitionRevision::Generate();
		fixture.Write(UpdatedDefinition(restoredRevision));
		const AssetChange validRestore = fixture.ObserveChange(AssetChangeKind::Modified);
		ActorImprintReloadResult restored = fixture.system.Reload(
			validRestore, SceneSpan(owners.data(), owners.size()));
		ReportUnexpectedReload(restored, ActorImprintReloadStatus::Reloaded);
		Check(validRestore.guid == fixture.assetGuid &&
			scene &&
			MetaFile::TryLoad((fixture.root / AssetPath).string()) == fixture.assetGuid &&
			restored.status == ActorImprintReloadStatus::Reloaded &&
			fixture.system.GetAvailability(fixture.imprint) == ActorImprintAvailability::Available &&
			scene.get() != ownerBefore &&
			VerifyUpdatedScene(
				*scene, state, "MissingAssetOverride", restoredRevision),
			"Validated same-GUID restoration leaves Missing and atomically reloads the live Scene");
		restored.FinalizeRetiredScenes();
	}

	void TestQueuedRemovedAddedBrokenRestoreKeepsMissingLatched()
	{
		Fixture fixture;
		const ActorImprint* oldDefinition = fixture.system.Resolve(fixture.imprint);
		if (!oldDefinition) return;
		SceneState state;
		auto scene = MakeScene(fixture, "QueuedMissingOverride", state, false, true);
		if (!state.originalRoot) return;
		SceneBase* ownerBefore = scene.get();
		std::array<std::unique_ptr<SceneBase>*, 1> owners{ &scene };

		json validSave;
		Check(SceneWriter::SerializeScene(scene.get(), validSave), "Queue regression Scene is publicly saveable before the Asset disappears");
		Check(std::filesystem::remove(fixture.root / AssetPath) &&
			fixture.assets.NotifyAssetChanged(AssetPath),
			"Queue regression records Removed without consuming it");
		fixture.WriteBroken();
		Check(fixture.assets.NotifyAssetChanged(AssetPath), "Queue regression records same-GUID Added over broken JSON");
		const auto changes = fixture.assets.TakePendingChanges();
		Check(changes.size() == 2 &&
			changes[0].kind == AssetChangeKind::Removed &&
			changes[1].kind == AssetChangeKind::Added &&
			changes[0].guid == fixture.assetGuid &&
			changes[1].guid == fixture.assetGuid &&
			fixture.assets.GetAssetEntry(fixture.assetGuid) != nullptr,
			"Pending queue preserves Removed-Added while the final catalog contains the same GUID");
		if (changes.size() != 2) return;

		ActorImprintReloadResult removedResult = fixture.system.Reload(
			changes[0], SceneSpan(owners.data(), owners.size()));
		Check(removedResult.status == ActorImprintReloadStatus::Failed &&
			fixture.system.GetAvailability(fixture.imprint) == ActorImprintAvailability::Missing,
			"Deferred Removed latches Missing even though final catalog lookup reaches broken JSON");
		ActorImprintReloadResult addedResult = fixture.system.Reload(
			changes[1], SceneSpan(owners.data(), owners.size()));
		Check(addedResult.status == ActorImprintReloadStatus::Failed &&
			fixture.system.GetAvailability(fixture.imprint) == ActorImprintAvailability::Missing &&
			fixture.system.Resolve(fixture.imprint) == oldDefinition &&
			scene.get() == ownerBefore &&
			scene->ResolveActor(state.rootGuid) == state.originalRoot,
			"Broken Added retry keeps Missing, the old definition, and the live Scene intact");

		json rejectedSave;
		Check(fixture.system.Load(fixture.assetGuid).IsNull() &&
			!fixture.system.Instantiate(*scene, fixture.imprint, {}) &&
			!SceneWriter::SerializeScene(scene.get(), rejectedSave),
			"Latched Missing rejects Load, Instantiate, and public Scene save after both failures");
	}

	void TestSameRevisionIsNoOp()
	{
		Fixture fixture;
		const ActorImprint* oldDefinition = fixture.system.Resolve(fixture.imprint);
		if (!oldDefinition) return;
		const DefinitionRevision revision = oldDefinition->GetRevision();
		SceneState state;
		auto scene = MakeScene(fixture, "NoOpOverride", state);
		if (!state.originalRoot) return;
		SceneBase* ownerBefore = scene.get();

		// The bytes and defaults differ, but DefinitionRevision is the semantic identity.
		fixture.Write(UpdatedDefinition(revision));
		const AssetChange change = fixture.ObserveChange(AssetChangeKind::Modified);
		std::array<std::unique_ptr<SceneBase>*, 1> owners{ &scene };
		ActorImprintReloadResult result = fixture.system.Reload(
			change, SceneSpan(owners.data(), owners.size()));

		Check(result.status == ActorImprintReloadStatus::NoChange &&
			result.previousRevision == revision &&
			result.currentRevision == revision &&
			result.affectedSceneIndices.empty(),
			"A validated candidate with the same DefinitionRevision is a no-op");
		Check(scene.get() == ownerBefore &&
			fixture.system.Resolve(fixture.imprint) == oldDefinition &&
			scene->ResolveActor(state.rootGuid) == state.originalRoot &&
			scene->ResolveActor(state.removedChildGuid) == state.originalRemovedChild &&
			state.originalRoot->GetName() == "NoOpOverride" &&
			state.originalRoot->IsActive(),
			"Same-revision no-op preserves Scene identity, Actor identity, and the published definition");
	}
}

int main()
{
	RegisterProbe();
	TestMultipleSceneAtomicSuccess();
	TestSceneCandidateFailureRollsBackEverything();
	TestIncompleteLiveSceneSet();
	TestRemovedMissingAndSameGuidRestore();
	TestQueuedRemovedAddedBrokenRestoreKeepsMissingLatched();
	TestSameRevisionIsNoOp();
	ComponentRegistry::Get().UnregisterAllGameComponents();
	return failures ? 1 : 0;
}
