#include "ActorImprint/ActorImprintAssetWorkflow.h"
#include "Command/EditorCommandHistory.h"
#include "Command/InstantiateActorImprintCommand.h"
#include "Document/EditorDocumentManager.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/ActorImprint/ActorImprintAssetDeserializer.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Scene/SceneBase.h"
#include "UI/AssetDragDropPayload.h"
#include "UI/ActorImprintsPanel.h"
#include "UI/HierarchyPanel.h"
#include "UI/Inspector/AssetPicker.h"
#include "UI/MenuBar.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
	int failures = 0;

	void Check(bool condition, const char* message)
	{
		if (condition) std::cout << "[PASS] " << message << '\n';
		else { ++failures; std::cerr << "[FAIL] " << message << '\n'; }
	}

	bool HasTransactionResidue(const std::filesystem::path& directory)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
			if (entry.path().filename().string().find(".101-") != std::string::npos) return true;
		return false;
	}

	struct Fixture
	{
		std::filesystem::path root = std::filesystem::temp_directory_path() /
			("101ImprintWorkflow-" + GuidGenerator::Generate().ToString());
		AssetManager assets;
		ActorImprintSystem system{assets};
		EngineContext context{};

		Fixture()
		{
			std::filesystem::create_directories(root);
			Check(assets.Initialize(root.string(), nullptr, nullptr),
				"Empty workflow asset catalog initializes");
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

	void NameValidation()
	{
		std::string normalized;
		ActorImprintAssetWorkflowError error;
		Check(ActorImprintAssetWorkflow::NormalizeFileName("Enemy", normalized, &error) &&
			normalized == "Enemy.imprint", "Create appends the canonical .imprint extension");
		Check(ActorImprintAssetWorkflow::NormalizeFileName("Enemy.IMPRINT", normalized, &error) &&
			normalized == "Enemy.imprint", "Create normalizes the ActorImprint extension case");
		Check(!ActorImprintAssetWorkflow::NormalizeFileName("", normalized, &error) &&
			error.code == ActorImprintAssetWorkflowErrorCode::InvalidName,
			"Create rejects an empty name with a diagnostic");
		Check(!ActorImprintAssetWorkflow::NormalizeFileName("folder/Enemy", normalized, &error),
			"Create rejects names that escape the asset-root destination");
		Check(!ActorImprintAssetWorkflow::NormalizeFileName("bad?.imprint", normalized, &error),
			"Create rejects invalid Windows filename characters");
		Check(!ActorImprintAssetWorkflow::NormalizeFileName("CON", normalized, &error) &&
			error.code == ActorImprintAssetWorkflowErrorCode::ReservedName,
			"Create rejects reserved Windows device names");
		Check(!ActorImprintAssetWorkflow::NormalizeFileName("trailing. ", normalized, &error),
			"Create rejects trailing dot or space names");
	}

	void WorkflowAndCommandBoundary()
	{
		Fixture fixture;
		Guid assetGuid;
		ActorImprintAssetWorkflowError error;
		ActorImprintsPanel panel;
		ActorImprintsPanel::Callbacks panelCallbacks;
		panelCallbacks.onCreate = [&](std::string_view name)
		{
			return ActorImprintAssetWorkflow::Create(
				name, fixture.assets, fixture.context, assetGuid, &error);
		};
		MenuBar::Callbacks menuCallbacks;
		menuCallbacks.onCreateActorImprint = [&panel]() { panel.RequestCreateDialog(); };
		Check(MenuBar::DispatchCreateActorImprint(menuCallbacks) &&
			panel.IsCreateDialogRequested(),
			"Assets menu intent opens the ActorImprintsPanel shared Create dialog");
		Check(ActorImprintsPanel::DispatchCreate(panelCallbacks, "Enemy") && assetGuid.IsValid(),
			"Panel Create intent reaches the production asset workflow callback");
		const auto assetPath = fixture.root / "ActorImprints" / "Enemy.imprint";
		Check(std::filesystem::is_regular_file(assetPath) &&
			std::filesystem::is_regular_file(assetPath.string() + ".meta") &&
			MetaFile::TryLoad(assetPath.string()) == assetGuid,
			"Create publishes the .imprint and matching .meta pair together");
		const AssetEntry* entry = fixture.assets.GetAssetEntry(assetGuid);
		Check(entry && entry->type == AssetType::ActorImprint &&
			entry->relativePath == "ActorImprints/Enemy.imprint",
			"Created pair is visible at the managed catalog path");

		ActorImprintAssetError assetError;
		auto definition = ActorImprintAssetDeserializer::Load(assetPath.string(), nullptr, &assetError);
		Check(definition && definition->GetActors().size() == 1 &&
			definition->GetRootActorId() == 1 && definition->GetNextLocalObjectId() == 3 &&
			definition->GetActors().front().components.size() == 1 &&
			definition->GetActors().front().components.front().id == 2,
			"Create uses the ET-16 default root model and initial LocalObjectID range");
		Check(!HasTransactionResidue(fixture.root),
			"Successful creation removes all same-directory staging files");

		const auto legacyPath = fixture.root / "Legacy.imprint";
		const Guid legacyGuid = GuidGenerator::Generate();
		std::filesystem::copy_file(assetPath, legacyPath);
		Check(MetaFile::Save(legacyPath.string(), legacyGuid) && fixture.assets.Refresh(),
			"Legacy root-level ActorImprint fixture is catalogued for compatibility filtering");
		const auto visibleEntries = ActorImprintsPanel::GetVisibleEntries(fixture.assets);
		Check(visibleEntries.size() == 1 && visibleEntries.front().guid == assetGuid &&
			!ActorImprintAssetWorkflow::IsManagedAssetPath("Legacy.imprint"),
			"Panel catalog model excludes ActorImprints outside the managed directory");

		Guid duplicateGuid;
		Check(!ActorImprintAssetWorkflow::Create(
			"enemy.IMPRINT", fixture.assets, fixture.context, duplicateGuid, &error) &&
			error.code == ActorImprintAssetWorkflowErrorCode::NameCollision &&
			fixture.assets.GetAssetEntries(AssetType::ActorImprint).size() == 2,
			"Case-insensitive collisions fail without changing the catalog or identity");

		const EditorAssetDragDropPayload correctPayload{assetGuid, AssetType::ActorImprint};
		Guid selected;
		Check(AssetPicker::TrySelectPayload(fixture.assets, AssetType::ActorImprint,
			correctPayload, {}, selected) && selected == assetGuid,
			"Typed picker accepts the shared AssetGUID plus AssetType payload");
		Check(!AssetPicker::TrySelectPayload(fixture.assets, AssetType::Texture,
			correctPayload, {}, selected), "Typed picker rejects a payload for another AssetType");
		Check(!AssetPicker::TrySelectPayload(fixture.assets, AssetType::ActorImprint,
			{GuidGenerator::Generate(), AssetType::ActorImprint}, {}, selected),
			"Typed picker rejects an unknown AssetGUID");

		EditorDocumentManager documents;
		EditorDocumentId documentId;
		panelCallbacks.onEdit = [&](const Guid& requestedGuid)
		{
			return ActorImprintAssetWorkflow::OpenDocument(requestedGuid, fixture.assets,
				fixture.system, fixture.context, documents, 1280, 720, documentId, &error);
		};
		Check(ActorImprintsPanel::DispatchEdit(panelCallbacks, assetGuid) &&
			documentId.IsValid() && documents.GetActiveDocument()->GetSourceAssetGuid() == assetGuid,
			"Panel Edit intent opens and activates the ActorImprint Document through ET-16");
		EditorDocumentId duplicateDocumentId;
		Check(ActorImprintAssetWorkflow::OpenDocument(assetGuid, fixture.assets,
			fixture.system, fixture.context, documents, 1280, 720, duplicateDocumentId, &error) &&
			duplicateDocumentId == documentId && documents.GetDocumentCount() == 1,
			"Edit deduplicates documents by AssetGUID");
		panelCallbacks.onDelete = [&](const Guid& requestedGuid)
		{
			return ActorImprintAssetWorkflow::Delete(
				requestedGuid, fixture.assets, fixture.system, documents, &error);
		};
		Check(!ActorImprintsPanel::DispatchDelete(panelCallbacks, assetGuid) &&
			error.code == ActorImprintAssetWorkflowErrorCode::OpenDocumentReference,
			"Panel Delete intent reports an open Document reference refusal");
		Check(documents.CloseDocument(documentId, EditorDocumentCloseDecision::Discard) ==
			EditorDocumentCloseResult::Closed, "The test closes the ActorImprint Document");

		SceneBase scene;
		scene.Initialize(fixture.context);
		Actor* ordinaryParent = scene.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Parent")));
		EditorCommandHistory history;
		auto instantiate = std::make_unique<InstantiateActorImprintCommand>(
			scene, fixture.system, assetGuid, ordinaryParent->GetGuid());
		auto* instantiateCommand = instantiate.get();
		Check(history.Execute(std::move(instantiate)) && history.GetUndoCount() == 1,
			"Hierarchy drop instantiates under an ordinary Actor and records one command");
		const Guid rootGuid = instantiateCommand->GetRootActorGuid();
		Actor* instanceRoot = scene.ResolveActor(rootGuid);
		Check(instanceRoot && instanceRoot->GetParent() == ordinaryParent &&
			fixture.system.GetLiveInstanceCount(assetGuid) == 1,
			"Instantiation preserves the explicit drop parent without transform adjustment");

		const std::size_t recordedCommands = history.GetUndoCount();
		Check(!history.Execute(std::make_unique<InstantiateActorImprintCommand>(
			scene, fixture.system, assetGuid, rootGuid)) &&
			history.GetUndoCount() == recordedCommands &&
			history.GetLastStructuralResult().reason == StructuralMutationReason::ImprintMemberImmutable,
			"Instance root/member targets are rejected without recording command history");
		Check(!history.Execute(std::make_unique<InstantiateActorImprintCommand>(
			scene, fixture.system, GuidGenerator::Generate())) &&
			history.GetUndoCount() == recordedCommands,
			"Missing assets fail without adding an Undo entry");

		Check(!ActorImprintAssetWorkflow::Delete(
			assetGuid, fixture.assets, fixture.system, documents, &error) &&
			error.code == ActorImprintAssetWorkflowErrorCode::LiveInstanceReference,
			"Delete rejects an ActorImprint used by a loaded Scene Instance");
		Check(history.Undo(), "Instantiation Undo destroys the Instance as one unit");
		scene.EditorUpdate(0.0f);
		Check(!scene.ResolveActor(rootGuid) && fixture.system.GetLiveInstanceCount(assetGuid) == 0,
			"Deferred Scene collection completes Instantiation Undo");
		Check(history.Redo(), "Instantiation Redo restores the captured Instance snapshot");
		instanceRoot = scene.ResolveActor(rootGuid);
		Check(instanceRoot && instanceRoot->GetGuid() == rootGuid &&
			instanceRoot->GetParent() == ordinaryParent,
			"Instantiation Redo preserves root ActorGUID and external parent identity");
		Check(history.Undo(), "The restored Instance can be undone again");
		scene.EditorUpdate(0.0f);

		fixture.assets.TakePendingChanges();
		Check(ActorImprintsPanel::DispatchDelete(panelCallbacks, assetGuid),
			"Panel Delete intent succeeds after all references are gone");
		Check(!std::filesystem::exists(assetPath) &&
			!std::filesystem::exists(assetPath.string() + ".meta") &&
			!fixture.assets.GetAssetEntry(assetGuid) && !HasTransactionResidue(fixture.root),
			"Delete removes the asset pair, catalog entry, and staging files together");
		const auto changes = fixture.assets.TakePendingChanges();
		Check(changes.size() == 1 && changes.front().kind == AssetChangeKind::Removed &&
			changes.front().guid == assetGuid,
			"Successful deletion emits one typed Removed catalog notification");
		scene.Finalize();
	}

	void DirectoryPreparationFailureLeavesNoAsset()
	{
		Fixture fixture;
		const auto managedPath = fixture.root / "ActorImprints";
		{
			std::ofstream blocker(managedPath);
			blocker << "not a directory";
		}
		Guid assetGuid;
		ActorImprintAssetWorkflowError error;
		Check(!ActorImprintAssetWorkflow::Create(
			"Blocked", fixture.assets, fixture.context, assetGuid, &error) &&
			error.code == ActorImprintAssetWorkflowErrorCode::FilesystemFailure &&
			!assetGuid.IsValid() && !std::filesystem::exists(managedPath / "Blocked.imprint") &&
			fixture.assets.GetAssetEntries(AssetType::ActorImprint).empty(),
			"directory preparation failure publishes no partial ActorImprint asset");
	}

	void EditorIntentRouting()
	{
		Fixture fixture;
		SceneBase ordinaryScene;
		ordinaryScene.Initialize(fixture.context);
		Actor* ordinaryRoot = ordinaryScene.AddRootActor(
			ActorFactory::CreateEmptyActor(Actor::InitDesc(true, TAG_NONE, "OrdinaryRoot")));
		Check(ordinaryRoot && !HierarchyPanel::ResolveEmptySpaceCreationParent(&ordinaryScene).IsValid(),
			"Hierarchy empty-space Create keeps root creation semantics in an ordinary Scene");

		SceneBase imprintScene;
		imprintScene.Initialize(fixture.context);
		Actor* imprintRoot = imprintScene.AddRootActor(
			ActorFactory::CreateEmptyActor(Actor::InitDesc(true, TAG_NONE, "ImprintRoot")));
		Check(imprintRoot && imprintScene.EnableSingleRootClosedSubtreePolicy() &&
			HierarchyPanel::ResolveEmptySpaceCreationParent(&imprintScene) == imprintRoot->GetGuid(),
			"Hierarchy empty-space Create resolves to the required root in an ActorImprint Working Scene");

		bool saved = false;
		MenuBar::Callbacks callbacks;
		callbacks.canSave = true;
		callbacks.onSaveDocument = [&saved]() { saved = true; };
		Check(MenuBar::DispatchSaveShortcut(callbacks, true, true, false) && saved,
			"Ctrl+S dispatches Save to the active Document when text input is inactive");
		saved = false;
		Check(!MenuBar::DispatchSaveShortcut(callbacks, true, true, true) && !saved,
			"Ctrl+S does not steal an active text-input edit");

		imprintScene.Finalize();
		ordinaryScene.Finalize();
	}
}

int main()
{
	NameValidation();
	WorkflowAndCommandBoundary();
	DirectoryPreparationFailureLeavesNoAsset();
	EditorIntentRouting();
	if (failures == 0)
	{
		std::cout << "ActorImprint workflow tests passed.\n";
		return 0;
	}
	std::cerr << failures << " ActorImprint workflow test(s) failed.\n";
	return 1;
}
