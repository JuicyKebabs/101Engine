#include "ActorImprint/ActorImprintAssetWorkflow.h"
#include "ActorImprint/ActorImprintEditingContext.h"
#include "Command/AddComponentCommand.h"
#include "Command/ComponentPropertyEditCommand.h"
#include "Command/CreateActorCommand.h"
#include "Command/InstantiateActorImprintCommand.h"
#include "Command/RenameActorCommand.h"
#include "Document/ActorImprintEditorDocument.h"
#include "Document/EditorDocumentManager.h"
#include "Document/SceneEditorDocument.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorReference.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/ActorImprint/ActorImprintInstanceRegistry.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Component.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetReference.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Scene/SceneWriter.h"
#include "nlohmann/json.hpp"

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace
{
	using json = nlohmann::json;
	constexpr const char* ProbeTypeName = "ActorImprintEndToEndProbeET18";
	int failures = 0;

	void Check(bool condition, const char* message)
	{
		if (condition) std::cout << "[PASS] " << message << '\n';
		else { ++failures; std::cerr << "[FAIL] " << message << '\n'; }
	}

	std::string ReadText(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary);
		return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
	}

	bool WriteText(const std::filesystem::path& path, std::string_view text)
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output.write(text.data(), static_cast<std::streamsize>(text.size()));
		return output.good();
	}

	class EndToEndProbe final : public Component
	{
	public:
		ActorReference target;
		AssetReference<ActorImprint> imprint;
		float weight = 1.0f;

		bool ResolveReferences(SceneBase& scene) override
		{
			return !target.HasValue() || target.Resolve(scene);
		}

	private:
		void OnAttachOverride() override {}
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	void RegisterProbe()
	{
		TypeMetadataBuilder<EndToEndProbe> builder(ProbeTypeName);
		builder.Property("target", &EndToEndProbe::target);
		builder.Property("imprint", &EndToEndProbe::imprint);
		builder.Property("weight", &EndToEndProbe::weight);
		ComponentRegistry::Get().RegisterGameComponent(
			ProbeTypeName, []() -> Component* { return new EndToEndProbe(); },
			typeid(EndToEndProbe), std::make_unique<TypeMetadata>(*builder.Build()));
	}

	struct Fixture
	{
		std::filesystem::path root = std::filesystem::temp_directory_path() /
			("101ImprintE2E-" + GuidGenerator::Generate().ToString());
		AssetManager assets;
		ActorImprintSystem system{assets};
		EngineContext context{};

		Fixture()
		{
			std::filesystem::create_directories(root);
			Check(assets.Initialize(root.string(), nullptr, nullptr),
				"E2E asset catalog initializes");
			context.pAssetManager = &assets;
			context.pActorImprintSystem = &system;
		}

		~Fixture()
		{
			system.Clear();
			std::error_code error;
			std::filesystem::remove_all(root, error);
		}
	};

	Actor* AddMainCamera(SceneBase& scene, const char* name)
	{
		return scene.AddRootActor(ActorFactory::CreateActor(ActorType::Camera,
			Actor::InitDesc(true, ActorTags::MainCamera, name)));
	}

	struct DefinitionIds
	{
		LocalObjectId root = InvalidLocalObjectId;
		LocalObjectId child = InvalidLocalObjectId;
		LocalObjectId grandchild = InvalidLocalObjectId;
		LocalObjectId firstProbe = InvalidLocalObjectId;
		LocalObjectId secondProbe = InvalidLocalObjectId;
	};

	struct SceneIds
	{
		Guid parent;
		Guid firstRoot;
		Guid firstGrandchild;
		Guid secondRoot;
		Guid secondGrandchild;
	};

	EndToEndProbe* ResolveProbe(SceneBase& scene, const Guid& rootGuid, LocalObjectId id)
	{
		Actor* root = scene.ResolveActor(rootGuid);
		return root ? static_cast<EndToEndProbe*>(
			scene.GetImprintInstances().ResolveComponent(root->GetHandle(), id)) : nullptr;
	}

	bool VerifyTwoInstanceScene(SceneBase& scene, const SceneIds& ids,
		const DefinitionIds& definitionIds, const Guid& assetGuid,
		std::string_view secondRootName, float secondWeight)
	{
		Actor* parent = scene.ResolveActor(ids.parent);
		Actor* firstRoot = scene.ResolveActor(ids.firstRoot);
		Actor* firstGrandchild = scene.ResolveActor(ids.firstGrandchild);
		Actor* secondRoot = scene.ResolveActor(ids.secondRoot);
		Actor* secondGrandchild = scene.ResolveActor(ids.secondGrandchild);
		auto* firstProbe = ResolveProbe(scene, ids.firstRoot, definitionIds.firstProbe);
		auto* secondProbe = ResolveProbe(scene, ids.secondRoot, definitionIds.firstProbe);
		if (!parent || !firstRoot || !firstGrandchild || !secondRoot ||
			!secondGrandchild || !firstProbe || !secondProbe) return false;
		return scene.GetImprintInstances().GetInstances().size() == 2 &&
			firstRoot->GetParent() == parent && secondRoot->GetParent() == parent &&
			firstRoot->GetName() == "OverriddenRoot" && secondRoot->GetName() == secondRootName &&
			firstProbe->weight == 7.0f && secondProbe->weight == secondWeight &&
			firstProbe->target.Resolve(scene) == secondGrandchild &&
			secondProbe->target.Resolve(scene) == secondGrandchild &&
			firstProbe->imprint.GetGuid() == assetGuid &&
			secondProbe->imprint.GetGuid() == assetGuid &&
			firstRoot->GetComponentsByExactType(typeid(EndToEndProbe)).size() == 2 &&
			secondRoot->GetComponentsByExactType(typeid(EndToEndProbe)).size() == 2;
	}

	bool ProcessSingleImprintChange(EditorDocumentManager& documents,
		Fixture& fixture, ActorImprintReloadStatus expected)
	{
		const auto changes = fixture.assets.TakePendingChanges();
		if (changes.size() != 1 || changes.front().type != AssetType::ActorImprint) return false;
		ActorImprintReloadResult result = documents.ReloadActorImprint(
			fixture.system, changes.front());
		const bool matched = result.status == expected;
		result.FinalizeRetiredScenes();
		return matched;
	}

	void CompleteLifecycle()
	{
		Fixture fixture;
		Guid assetGuid;
		ActorImprintAssetWorkflowError workflowError;
		Check(ActorImprintAssetWorkflow::Create(
			"EndToEnd", fixture.assets, fixture.context, assetGuid, &workflowError),
			"E2E Create publishes the new ActorImprint");
		fixture.assets.TakePendingChanges();

		EditorDocumentManager documents;
		EditorDocumentId imprintDocumentId;
		Check(ActorImprintAssetWorkflow::OpenDocument(assetGuid, fixture.assets,
			fixture.system, fixture.context, documents, 1280, 720,
			imprintDocumentId, &workflowError), "E2E Edit opens the new definition");
		auto* imprintDocument = static_cast<ActorImprintEditorDocument*>(
			documents.GetActiveDocument());
		SceneBase* editingScene = imprintDocument ? imprintDocument->GetWorkingScene() : nullptr;
		Actor* editingRoot = editingScene ? editingScene->GetRootActors().front() : nullptr;
		if (!imprintDocument || !editingScene || !editingRoot)
		{
			Check(false, "E2E editing context is available");
			return;
		}

		auto createChild = std::make_unique<CreateActorCommand>(editingScene,
			Actor::InitDesc(true, TAG_NONE, "Child"), editingRoot->GetGuid());
		auto* createChildCommand = createChild.get();
		Check(imprintDocument->ExecuteCommand(std::move(createChild)),
			"E2E authoring adds a child through Document history");
		const Guid childGuid = createChildCommand->GetActorGuid();
		auto createGrandchild = std::make_unique<CreateActorCommand>(editingScene,
			Actor::InitDesc(true, TAG_NONE, "Grandchild"), childGuid);
		auto* createGrandchildCommand = createGrandchild.get();
		Check(imprintDocument->ExecuteCommand(std::move(createGrandchild)),
			"E2E authoring adds a deeper hierarchy level");
		const Guid grandchildGuid = createGrandchildCommand->GetActorGuid();
		Check(imprintDocument->ExecuteCommand(std::make_unique<AddComponentCommand>(
			editingScene, editingRoot->GetGuid(), ProbeTypeName)) &&
			imprintDocument->ExecuteCommand(std::make_unique<AddComponentCommand>(
			editingScene, editingRoot->GetGuid(), ProbeTypeName)),
			"E2E authoring adds multiple Components of the same registered type");

		auto probes = editingRoot->GetComponentsByExactType(typeid(EndToEndProbe));
		auto* firstProbe = probes.size() == 2 ? static_cast<EndToEndProbe*>(probes[0]) : nullptr;
		auto* secondProbe = probes.size() == 2 ? static_cast<EndToEndProbe*>(probes[1]) : nullptr;
		const auto targetPath = PropertyPath::FromMembers({"target"});
		const auto imprintPath = PropertyPath::FromMembers({"imprint"});
		ActorReference internalReference;
		internalReference.SetGuid(grandchildGuid);
		const ComponentPropertyIdentity targetIdentity{
			editingRoot->GetGuid(), typeid(EndToEndProbe), 0, *targetPath};
		const ComponentPropertyIdentity imprintIdentity{
			editingRoot->GetGuid(), typeid(EndToEndProbe), 0, *imprintPath};
		Check(firstProbe && secondProbe &&
			imprintDocument->ExecuteCommand(std::make_unique<ComponentPropertyEditCommand>(
				editingScene, targetIdentity, PropertyValue(ActorReference{}),
				PropertyValue(internalReference))) &&
			imprintDocument->ExecuteCommand(std::make_unique<ComponentPropertyEditCommand>(
				editingScene, imprintIdentity, PropertyValue(firstProbe->imprint.ToValue()),
				PropertyValue(AssetReferenceValue{assetGuid, AssetType::ActorImprint, true}))),
			"E2E authoring stores internal ActorReference and typed AssetReference defaults");

		auto& objectMap = imprintDocument->GetEditingContext()->GetObjectMap();
		DefinitionIds definitionIds{
			objectMap.FindActor(editingRoot->GetGuid()),
			objectMap.FindActor(childGuid),
			objectMap.FindActor(grandchildGuid),
			objectMap.FindComponent(firstProbe),
			objectMap.FindComponent(secondProbe),
		};
		Check(definitionIds.root != InvalidLocalObjectId &&
			definitionIds.grandchild != InvalidLocalObjectId &&
			definitionIds.firstProbe != InvalidLocalObjectId &&
			definitionIds.secondProbe != InvalidLocalObjectId,
			"E2E authoring allocates stable LocalObjectIDs for the complete graph");
		Check(imprintDocument->Save(), "E2E Save commits the authored definition");
		Check(ProcessSingleImprintChange(documents, fixture, ActorImprintReloadStatus::Reloaded),
			"E2E safe-point reload publishes the first authored revision");
		Check(documents.CloseDocument(imprintDocumentId, EditorDocumentCloseDecision::Discard) ==
			EditorDocumentCloseResult::Closed, "E2E closes the clean editing Document");

		const std::filesystem::path scenePath = fixture.root / "EndToEnd.scene";
		auto firstSceneOwner = std::make_unique<SceneBase>();
		firstSceneOwner->Initialize(fixture.context);
		Actor* firstParent = AddMainCamera(*firstSceneOwner, "EditorParent");
		auto firstSceneDocumentOwner = std::make_unique<SceneEditorDocument>(
			std::move(firstSceneOwner), scenePath.string(), 1280, 720);
		auto* firstSceneDocument = firstSceneDocumentOwner.get();
		documents.AddDocument(std::move(firstSceneDocumentOwner));
		SceneBase* firstScene = firstSceneDocument->GetWorkingScene();

		auto firstInstantiate = std::make_unique<InstantiateActorImprintCommand>(
			*firstScene, fixture.system, assetGuid, firstParent->GetGuid());
		if (!firstInstantiate->Execute())
		{
			std::cerr << "First instantiate diagnostic: "
				<< firstInstantiate->GetErrorMessage() << '\n';
			Check(false, "Editor workflow instantiates the first Instance through command history");
			return;
		}
		const Guid firstRootGuid = firstInstantiate->GetRootActorGuid();
		Check(firstSceneDocument->RecordExecutedCommand(std::move(firstInstantiate)),
			"Editor workflow instantiates the first Instance through command history");
		auto secondInstantiate = std::make_unique<InstantiateActorImprintCommand>(
			*firstScene, fixture.system, assetGuid, firstParent->GetGuid());
		if (!secondInstantiate->Execute())
		{
			std::cerr << "Second instantiate diagnostic: "
				<< secondInstantiate->GetErrorMessage() << '\n';
			Check(false, "Editor workflow instantiates an independent second Instance");
			return;
		}
		const Guid secondRootGuid = secondInstantiate->GetRootActorGuid();
		Check(firstSceneDocument->RecordExecutedCommand(std::move(secondInstantiate)),
			"Editor workflow instantiates an independent second Instance");
		Actor* firstRoot = firstScene->ResolveActor(firstRootGuid);
		Actor* secondRoot = firstScene->ResolveActor(secondRootGuid);
		Actor* firstGrandchild = firstRoot ? firstScene->GetImprintInstances().ResolveActor(
			firstRoot->GetHandle(), definitionIds.grandchild) : nullptr;
		Actor* secondGrandchild = secondRoot ? firstScene->GetImprintInstances().ResolveActor(
			secondRoot->GetHandle(), definitionIds.grandchild) : nullptr;
		firstProbe = firstRoot ? ResolveProbe(*firstScene, firstRootGuid, definitionIds.firstProbe) : nullptr;
		ActorReference crossInstanceReference;
		if (secondGrandchild) crossInstanceReference.SetGuid(secondGrandchild->GetGuid());
		const auto weightPath = PropertyPath::FromMembers({"weight"});
		Check(firstProbe && firstGrandchild && secondGrandchild &&
			firstSceneDocument->ExecuteCommand(std::make_unique<ComponentPropertyEditCommand>(
				firstScene, ComponentPropertyIdentity{firstRootGuid, typeid(EndToEndProbe), 0, *weightPath},
				PropertyValue(1.0f), PropertyValue(7.0f))) &&
			firstSceneDocument->ExecuteCommand(std::make_unique<ComponentPropertyEditCommand>(
				firstScene, ComponentPropertyIdentity{firstRootGuid, typeid(EndToEndProbe), 0, *targetPath},
				PropertyValue(firstProbe->target), PropertyValue(crossInstanceReference))) &&
			firstSceneDocument->ExecuteCommand(std::make_unique<RenameActorCommand>(
				firstScene, firstRootGuid, "OverriddenRoot")),
			"Instance property overrides include scalar, cross-Instance reference, and Actor name");

		SceneIds sceneIds{
			firstParent->GetGuid(), firstRootGuid, firstGrandchild->GetGuid(),
			secondRootGuid, secondGrandchild->GetGuid(),
		};
		Check(firstSceneDocument->Save(), "Scene Save persists ordinary and Instance records");
		json savedScene;
		Check(SceneWriter::SerializeScene(firstScene, savedScene) &&
			savedScene["actorImprintInstances"].size() == 2,
			"Scene v4 save contains two Instance records without member duplication");
		const std::string firstSavedSceneBytes = ReadText(scenePath);

		auto secondSceneOwner = std::make_unique<SceneBase>();
		secondSceneOwner->Initialize(fixture.context);
		Actor* runtimeParent = AddMainCamera(*secondSceneOwner, "RuntimeParent");
		AssetReference<ActorImprint> runtimeReference;
		runtimeReference.SetGuid(assetGuid);
		Actor* runtimeRoot = fixture.system.Instantiate(
			*secondSceneOwner, runtimeReference, runtimeParent->GetHandle());
		const Guid runtimeRootGuid = runtimeRoot ? runtimeRoot->GetGuid() : Guid{};
		Check(runtimeRoot && ResolveProbe(*secondSceneOwner, runtimeRootGuid,
			definitionIds.firstProbe),
			"Game/runtime API instantiates from AssetReference<ActorImprint>");
		auto secondSceneDocumentOwner = std::make_unique<SceneEditorDocument>(
			std::move(secondSceneOwner), (fixture.root / "Runtime.scene").string(), 1280, 720);
		auto* secondSceneDocument = secondSceneDocumentOwner.get();
		documents.AddDocument(std::move(secondSceneDocumentOwner), false);

		{
			AssetManager restartAssets;
			ActorImprintSystem restartSystem{restartAssets};
			EngineContext restartContext{};
			Check(restartAssets.Initialize(fixture.root.string(), nullptr, nullptr),
				"Process-restart catalog rediscovers the persisted asset identity");
			restartContext.pAssetManager = &restartAssets;
			restartContext.pActorImprintSystem = &restartSystem;
			SceneLoadResult restarted = SceneLoader::LoadCandidate(scenePath.string(), restartContext);
			json restartedSave;
			Check(restarted && VerifyTwoInstanceScene(*restarted.scene, sceneIds, definitionIds,
				assetGuid, "Root", 1.0f),
				"Process-restart Scene Load restores hierarchy, identities, overrides, and references");
			Check(restarted && SceneWriter::SerializeScene(restarted.scene.get(), restartedSave) &&
				restartedSave.dump(4) == savedScene.dump(4),
				"The E2E Scene golden is deterministic across Save-Load-Save");
			const auto roundTripPath = fixture.root / "EndToEnd-RoundTrip.scene";
			Check(restarted && SceneWriter::SaveScene(roundTripPath.string(), restarted.scene.get()) &&
				ReadText(roundTripPath) == firstSavedSceneBytes,
				"Two persisted Scene saves are byte-identical on the Windows filesystem");
			if (restarted.scene) restarted.scene->Finalize();
			Check(restartSystem.Clear(), "Restart System releases its loaded definition");
		}

		Check(ActorImprintAssetWorkflow::OpenDocument(assetGuid, fixture.assets,
			fixture.system, fixture.context, documents, 1280, 720,
			imprintDocumentId, &workflowError), "Reload stage reopens the definition Document");
		imprintDocument = static_cast<ActorImprintEditorDocument*>(documents.GetActiveDocument());
		editingScene = imprintDocument->GetWorkingScene();
		editingRoot = editingScene->GetRootActors().front();
		firstProbe = static_cast<EndToEndProbe*>(
			imprintDocument->GetEditingContext()->GetObjectMap().FindComponent(definitionIds.firstProbe));
		Check(imprintDocument->ExecuteCommand(std::make_unique<RenameActorCommand>(
			editingScene, editingRoot->GetGuid(), "ReloadedRoot")) &&
			firstProbe && imprintDocument->ExecuteCommand(
				std::make_unique<ComponentPropertyEditCommand>(editingScene,
					ComponentPropertyIdentity{editingRoot->GetGuid(), typeid(EndToEndProbe), 0, *weightPath},
					PropertyValue(1.0f), PropertyValue(2.0f))) &&
			imprintDocument->Save(),
			"Definition edit produces a new default revision while Instances are loaded");
		const auto imprintAssetPath = fixture.root / "ActorImprints" / "EndToEnd.imprint";
		const std::string validReloadBytes = ReadText(imprintAssetPath);
		SceneBase* firstSceneBeforeReload = firstSceneDocument->GetWorkingScene();
		SceneBase* secondSceneBeforeReload = secondSceneDocument->GetWorkingScene();
		const DefinitionRevision previousRevision = fixture.system.Resolve(
			fixture.system.FindHandle(assetGuid))->GetRevision();
		const auto reloadChanges = fixture.assets.TakePendingChanges();
		if (reloadChanges.size() != 1)
		{
			Check(false, "Definition save emits exactly one reload notification");
			return;
		}
		ActorImprintReloadResult reload = documents.ReloadActorImprint(
			fixture.system, reloadChanges.front());
		Check(reload.status == ActorImprintReloadStatus::Reloaded &&
			reload.previousRevision == previousRevision && reload.currentRevision != previousRevision &&
			reload.affectedSceneIndices.size() == 2,
			"Reload atomically migrates every loaded Scene to the new DefinitionRevision");
		firstScene = firstSceneDocument->GetWorkingScene();
		SceneBase* secondScene = secondSceneDocument->GetWorkingScene();
		Check(firstScene != firstSceneBeforeReload && secondScene != secondSceneBeforeReload &&
			VerifyTwoInstanceScene(*firstScene, sceneIds, definitionIds, assetGuid,
				"ReloadedRoot", 2.0f),
			"Reload keeps overridden values and GUIDs while adopting new defaults");
		Actor* migratedRuntimeRoot = secondScene->ResolveActor(runtimeRootGuid);
		auto* migratedRuntimeProbe = ResolveProbe(
			*secondScene, runtimeRootGuid, definitionIds.firstProbe);
		Check(migratedRuntimeRoot && migratedRuntimeRoot->GetName() == "ReloadedRoot" &&
			migratedRuntimeProbe && migratedRuntimeProbe->weight == 2.0f,
			"A second loaded Scene adopts the same new defaults");
		Check(!firstSceneDocument->GetCommandHistory().CanUndo() && !firstSceneDocument->Undo(),
			"Successful Scene replacement clears stale Undo history at the reload boundary");
		reload.FinalizeRetiredScenes();
		Check(documents.CloseDocument(imprintDocumentId, EditorDocumentCloseDecision::Discard) ==
			EditorDocumentCloseResult::Closed, "Reload stage closes the saved definition Document");

		Actor* migratedParent = firstScene->ResolveActor(sceneIds.parent);
		Check(firstSceneDocument->ExecuteCommand(std::make_unique<RenameActorCommand>(
			firstScene, migratedParent->GetGuid(), "ParentAfterReload")),
			"Failure injection establishes post-reload Editor history");
		SceneBase* firstBeforeFailure = firstSceneDocument->GetWorkingScene();
		SceneBase* secondBeforeFailure = secondSceneDocument->GetWorkingScene();
		const ActorImprint* definitionBeforeFailure = fixture.system.Resolve(
			fixture.system.FindHandle(assetGuid));
		Check(WriteText(imprintAssetPath, "{ broken") &&
			fixture.assets.NotifyAssetChanged("ActorImprints/EndToEnd.imprint"),
			"Failure injection publishes a corrupt candidate notification");
		const auto brokenChanges = fixture.assets.TakePendingChanges();
		const auto brokenChange = std::find_if(brokenChanges.begin(), brokenChanges.end(),
			[&](const AssetChange& change)
			{
				return change.guid == assetGuid && change.type == AssetType::ActorImprint;
			});
		if (brokenChange == brokenChanges.end() ||
			std::count_if(brokenChanges.begin(), brokenChanges.end(), [&](const AssetChange& change)
			{
				return change.guid == assetGuid && change.type == AssetType::ActorImprint;
			}) != 1)
		{
			Check(false, "Corrupt replacement emits exactly one reload notification");
			return;
		}
		ActorImprintReloadResult rejected = documents.ReloadActorImprint(
			fixture.system, *brokenChange);
		Check(rejected.status == ActorImprintReloadStatus::Failed &&
			rejected.error.code == ActorImprintReloadErrorCode::CandidateAssetFailed &&
			firstSceneDocument->GetWorkingScene() == firstBeforeFailure &&
			secondSceneDocument->GetWorkingScene() == secondBeforeFailure &&
			fixture.system.Resolve(fixture.system.FindHandle(assetGuid)) == definitionBeforeFailure &&
			firstSceneDocument->GetCommandHistory().CanUndo(),
			"Corrupt reload preserves published definition, every Scene, and Editor history");
		Check(WriteText(imprintAssetPath, validReloadBytes) &&
			fixture.assets.NotifyAssetChanged("ActorImprints/EndToEnd.imprint") &&
			ProcessSingleImprintChange(documents, fixture, ActorImprintReloadStatus::NoChange),
			"Restoring identical validated bytes is a safe same-revision no-op");

		{
			SceneBase performanceScene;
			performanceScene.Initialize(fixture.context);
			Actor* performanceParent = AddMainCamera(performanceScene, "PerformanceParent");
			const ActorImprintHandle handle = fixture.system.FindHandle(assetGuid);
			constexpr int instanceCount = 64;
			const auto start = std::chrono::steady_clock::now();
			bool instantiated = true;
			for (int i = 0; i < instanceCount; ++i)
				instantiated = fixture.system.Instantiate(
					performanceScene, handle, performanceParent->GetHandle()) != nullptr && instantiated;
			json performanceSave;
			const bool serialized = SceneWriter::SerializeScene(&performanceScene, performanceSave);
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - start).count();
			Check(instantiated && serialized &&
				performanceScene.GetImprintInstances().GetInstances().size() == instanceCount,
				"Representative scale run instantiates and serializes 64 three-Actor Instances");
			std::cout << "[OBSERVATION] 64 Instance materializations plus Scene serialization: "
				<< elapsed << " ms\n";
			performanceScene.Finalize();
		}

		Check(!ActorImprintAssetWorkflow::Delete(assetGuid, fixture.assets,
			fixture.system, documents, &workflowError) &&
			workflowError.code == ActorImprintAssetWorkflowErrorCode::LiveInstanceReference,
			"Final Delete remains blocked while either loaded Scene owns an Instance");
		documents.Clear();
		Check(fixture.system.GetLiveInstanceCount(assetGuid) == 0,
			"Closing all Scene Documents releases every definition pin");
		Check(ActorImprintAssetWorkflow::Delete(assetGuid, fixture.assets,
			fixture.system, documents, &workflowError) &&
			!fixture.assets.GetAssetEntry(assetGuid) &&
			!std::filesystem::exists(imprintAssetPath) &&
			!std::filesystem::exists(imprintAssetPath.string() + ".meta"),
			"E2E Delete removes the asset only after every reference constraint is clear");
	}
}

int main()
{
	RegisterProbe();
	CompleteLifecycle();
	ComponentRegistry::Get().UnregisterAllGameComponents();
	if (failures == 0)
	{
		std::cout << "ActorImprint end-to-end tests passed.\n";
		return 0;
	}
	std::cerr << failures << " ActorImprint end-to-end test(s) failed.\n";
	return 1;
}
