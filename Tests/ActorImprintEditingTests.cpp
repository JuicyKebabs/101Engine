#include "ActorImprint/ActorImprintEditingContext.h"
#include "Command/AddComponentCommand.h"
#include "Command/ComponentPropertyEditCommand.h"
#include "Command/CreateActorCommand.h"
#include "Command/ChangeActorTagCommand.h"
#include "Command/DeleteActorCommand.h"
#include "Command/RemoveComponentCommand.h"
#include "Command/ReparentActorCommand.h"
#include "Command/RenameActorCommand.h"
#include "Document/ActorImprintEditorDocument.h"
#include "Document/EditorDocumentManager.h"
#include "Tag/TagManagementWorkflow.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorReference.h"
#include "Engine/ActorImprint/ActorImprintAssetDeserializer.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Component.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Project/ProjectSettings.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace
{
	int failures = 0;
	void Check(bool condition, const char* message)
	{
		if (condition) std::cout << "[PASS] " << message << '\n';
		else { ++failures; std::cerr << "[FAIL] " << message << '\n'; }
	}

	std::string ReadText(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary);
		return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	}

	bool HasTransactionResidue(const std::filesystem::path& directory)
	{
		for (const auto& entry : std::filesystem::directory_iterator(directory))
			if (entry.path().filename().string().find(".101-") != std::string::npos) return true;
		return false;
	}

	struct Fixture
	{
		std::filesystem::path root = std::filesystem::temp_directory_path() /
			("101ImprintEditing-" + GuidGenerator::Generate().ToString());
		Guid assetGuid = GuidGenerator::Generate();
		AssetManager assets;
		ActorImprintSystem system{assets};
		EngineContext engineContext{};

		Fixture()
		{
			std::filesystem::create_directories(root);
			std::ifstream source("Tests/Fixtures/ActorImprint/Minimal.imprint", std::ios::binary);
			std::ofstream destination(root / "Editable.imprint", std::ios::binary);
			destination << source.rdbuf();
			destination.close();
			Check(MetaFile::Save((root / "Editable.imprint").string(), assetGuid),
				"Editing fixture writes stable asset metadata");
			Check(assets.Initialize(root.string(), nullptr, nullptr),
				"Editing fixture catalog initializes");
			engineContext.pAssetManager = &assets;
			engineContext.pActorImprintSystem = &system;
		}

		~Fixture()
		{
			system.Clear();
			std::error_code error;
			std::filesystem::remove_all(root, error);
		}

		std::unique_ptr<ActorImprintEditingContext> Open()
		{
			ActorImprintEditingOpenError error;
			auto context = ActorImprintEditingContext::Open(
				assetGuid, assets, system, engineContext, &error);
			if (!context) std::cerr << error.path << ": " << error.message << '\n';
			return context;
		}
	};

	void OpenAndPolicy()
	{
		Fixture fixture;
		auto context = fixture.Open();
		Check(context != nullptr, "Validated definition opens as an editing context");
		if (!context) return;
		SceneBase* scene = context->GetWorkingScene();
		auto roots = scene->GetRootActors();
		Check(roots.size() == 1 && roots.front()->GetName() == "Root",
			"Working Scene expands the complete definition with one root");
		Check(scene->GetImprintInstances().GetInstances().empty(),
			"Working Scene expansion does not create runtime Instance provenance");
		Check(context->GetObjectMap().FindActor(roots.front()->GetGuid()) == 10 &&
			context->GetObjectMap().FindComponent(roots.front()->GetComponentByClass<Transform>()) == 11 &&
			context->GetObjectMap().GetNextLocalObjectId() == 12,
			"Definition LocalObjectIDs initialize the bidirectional editing map");

		StructuralMutationResult result;
		Check(scene->CanAddRootActor().reason == StructuralMutationReason::SingleRootInvariant &&
			!scene->AddRootActor(ActorFactory::CreateEmptyActor({}), &result) &&
			result.reason == StructuralMutationReason::SingleRootInvariant,
			"Single-root policy matches preflight and execution for a second root");
		Actor* root = roots.front();
		Check(scene->CanDestroy(root).reason == StructuralMutationReason::SingleRootInvariant &&
			!scene->RemoveActor(root, true, &result) &&
			result.reason == StructuralMutationReason::SingleRootInvariant,
			"Single-root policy matches preflight and execution for root deletion");
	}

	void CommandIdentityAndSnapshot()
	{
		Fixture fixture;
		auto context = fixture.Open();
		if (!context) { Check(false, "Command fixture opens"); return; }
		auto document = std::make_unique<ActorImprintEditorDocument>(std::move(context), 1280, 720);
		auto* editing = document->GetEditingContext();
		SceneBase* scene = document->GetWorkingScene();
		Actor* root = scene->GetRootActors().front();

		auto create = std::make_unique<CreateActorCommand>(scene,
			Actor::InitDesc(true, TAG_NONE, "Child"), root->GetGuid());
		CreateActorCommand* createCommand = create.get();
		Check(document->ExecuteCommand(std::move(create)),
			"Existing CreateActorCommand executes through the editing wrapper");
		const Guid childGuid = createCommand->GetActorGuid();
		Actor* child = scene->ResolveActor(childGuid);
		const LocalObjectId childId = editing->GetObjectMap().FindActor(childGuid);
		const LocalObjectId transformId = editing->GetObjectMap().FindComponent(
			child ? child->GetComponentByClass<Transform>() : nullptr);
		Check(child && childId == 12 && transformId == 13 &&
			editing->GetObjectMap().GetNextLocalObjectId() == 14,
			"Actor creation assigns stable IDs to the Actor and required Transform");
		Check(scene->CanReparent(child, nullptr).reason == StructuralMutationReason::SingleRootInvariant,
			"Working Scene refuses moving a child outside the closed subtree");

		Check(document->Undo() && editing->GetObjectMap().FindActor(childGuid) == 0 &&
			editing->GetObjectMap().GetNextLocalObjectId() == 14,
			"Create Undo removes mappings without reusing issued IDs");
		scene->EditorUpdate(0.0f);
		Check(document->Redo(), "Create Redo restores the Actor through the reused command");
		child = scene->ResolveActor(childGuid);
		Check(child && editing->GetObjectMap().FindActor(childGuid) == childId &&
			editing->GetObjectMap().FindComponent(child->GetComponentByClass<Transform>()) == transformId,
			"Create Redo restores the original LocalObjectIDs onto new runtime objects");

		Check(document->ExecuteCommand(std::make_unique<AddComponentCommand>(scene, childGuid, "Camera")),
			"Existing AddComponentCommand executes through the editing wrapper");
		Camera* camera = child->GetComponentByClass<Camera>();
		const LocalObjectId cameraId = editing->GetObjectMap().FindComponent(camera);
		Check(camera && cameraId == 14 && editing->GetObjectMap().GetNextLocalObjectId() == 15,
			"Component creation assigns one persistent LocalObjectID");
		Check(document->Undo(), "Component Add Undo succeeds");
		Check(!child->GetComponentByClass<Camera>() && editing->GetObjectMap().FindComponent(cameraId) == nullptr &&
			editing->GetObjectMap().GetNextLocalObjectId() == 15,
			"Component Add Undo removes the binding and keeps nextLocalObjectId monotonic");
		Check(document->Redo(), "Component Add Redo succeeds");
		camera = child->GetComponentByClass<Camera>();
		Check(camera && editing->GetObjectMap().FindComponent(camera) == cameraId,
			"Component Add Redo rebinds the original ID to the restored pointer");

		const auto targetPath = PropertyPath::FromMembers({"targetActorId"});
		ActorReference localReference;
		localReference.SetGuid(root->GetGuid());
		ComponentPropertyIdentity target{ childGuid, typeid(Camera), 0, *targetPath };
		Check(ApplyComponentPropertyValue(scene, target, PropertyValue(localReference)),
			"Working Scene accepts Actor references inside its closed subtree");
		SceneBase foreignScene;
		Actor* foreign = foreignScene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		ActorReference externalReference;
		externalReference.SetGuid(foreign->GetGuid());
		Check(scene->CanReferenceActor(foreign->GetGuid()).reason == StructuralMutationReason::ExternalActorReference &&
			!ApplyComponentPropertyValue(scene, target, PropertyValue(externalReference)),
			"External Actor reference policy rejects both preflight and command application");

		nlohmann::json saved;
		ActorImprintEditingSnapshotError snapshotError;
		auto snapshot = editing->CaptureSnapshot(saved, &snapshotError);
		Check(snapshot != nullptr && saved["rootActorLocalObjectId"] == 10 &&
			saved["nextLocalObjectId"] == 15 && saved["actors"].size() == 2,
			"Working Scene and ObjectMap produce a complete immutable snapshot");
		ActorImprintAssetError roundTripError;
		auto roundTrip = ActorImprintAssetDeserializer::Deserialize(saved, nullptr, &roundTripError);
		Check(roundTrip != nullptr,
			"Captured editing snapshot passes the ET-08 asset validator");

		Check(document->ExecuteCommand(std::make_unique<RemoveComponentCommand>(
			scene, childGuid, "Camera", 0)), "Existing RemoveComponentCommand executes through the wrapper");
		Check(!editing->GetObjectMap().FindComponent(cameraId),
			"Component removal clears its runtime binding");
		Check(document->Undo(), "Component removal Undo restores the Component");
		camera = child->GetComponentByClass<Camera>();
		Check(camera && editing->GetObjectMap().FindComponent(camera) == cameraId,
			"Component removal Undo preserves its LocalObjectID");

		Check(document->ExecuteCommand(std::make_unique<DeleteActorCommand>(scene, childGuid)),
			"Existing DeleteActorCommand executes through the wrapper");
		Check(editing->GetObjectMap().FindActor(childGuid) == 0,
			"Subtree deletion removes Actor and Component bindings together");
		Check(!document->Undo(), "Delete Undo preserves history until deferred garbage collection finishes");
		scene->EditorUpdate(0.0f);
		Check(document->Undo(), "Delete Undo restores the subtree after collection");
		child = scene->ResolveActor(childGuid);
		camera = child ? child->GetComponentByClass<Camera>() : nullptr;
		Check(child && editing->GetObjectMap().FindActor(childGuid) == childId && camera &&
			editing->GetObjectMap().FindComponent(camera) == cameraId,
			"Subtree deletion Undo restores every previous LocalObjectID");
	}

	void DocumentIsolationAndDeduplication()
	{
		Fixture fixture;
		auto firstContext = fixture.Open();
		auto secondContext = fixture.Open();
		if (!firstContext || !secondContext) { Check(false, "Duplicate document fixture opens"); return; }
		EditorDocumentManager manager;
		auto first = std::make_unique<ActorImprintEditorDocument>(std::move(firstContext), 800, 600);
		ActorImprintEditorDocument* firstPointer = first.get();
		const EditorDocumentId firstId = manager.AddDocument(std::move(first));
		const EditorDocumentId duplicateId = manager.AddDocument(
			std::make_unique<ActorImprintEditorDocument>(std::move(secondContext), 800, 600));
		Check(firstId.IsValid() && duplicateId == firstId && manager.GetDocumentCount() == 1 &&
			manager.GetActiveDocument() == firstPointer,
			"AssetGUID deduplicates ActorImprint documents and activates the existing document");
		Check(!firstPointer->CanEnterPlay(), "ActorImprint document refuses Play mode");
	}

	void DefaultRootModel()
	{
		Fixture fixture;
		nlohmann::json serialized;
		ActorImprintEditingSnapshotError error;
		auto definition = ActorImprintEditingSnapshot::CreateDefault(
			fixture.assets, fixture.engineContext, serialized, &error);
		Check(definition != nullptr && definition->GetRootActorId() == 1 &&
			definition->GetNextLocalObjectId() == 3 && definition->GetActors().size() == 1,
			"Default model contains one root with the initial LocalObjectID range");
		if (!definition) return;
		const auto& root = definition->GetActors().front();
		Check(root.parentId == InvalidLocalObjectId && root.properties["name"] == "Root" &&
			root.components.size() == 1 && root.components.front().id == 2 &&
			root.components.front().typeName == "Transform",
			"Default model is a root Actor with only its required Transform");
		ActorImprintAssetError roundTripError;
		Check(ActorImprintAssetDeserializer::Deserialize(serialized, nullptr, &roundTripError) != nullptr,
			"Default model passes the ET-08 validator through the normal snapshot path");
	}

	void SaveAndReload()
	{
		Fixture fixture;
		auto context = fixture.Open();
		if (!context) { Check(false, "Save fixture opens"); return; }
		const ActorImprintHandle handle = fixture.system.FindHandle(fixture.assetGuid);
		const ActorImprint* originalDefinition = fixture.system.Resolve(handle);
		if (!originalDefinition) { Check(false, "Save fixture retains loaded definition"); return; }
		const DefinitionRevision originalRevision = originalDefinition->GetRevision();

		EditorDocumentManager manager;
		auto documentOwner = std::make_unique<ActorImprintEditorDocument>(
			std::move(context), 1280, 720);
		ActorImprintEditorDocument* document = documentOwner.get();
		manager.AddDocument(std::move(documentOwner));
		fixture.assets.TakePendingChanges();

		SceneBase* scene = document->GetWorkingScene();
		Actor* root = scene->GetRootActors().front();
		const LocalObjectId rootId = document->GetEditingContext()->GetObjectMap().FindActor(root->GetGuid());
		const LocalObjectId transformId = document->GetEditingContext()->GetObjectMap().FindComponent(
			root->GetComponentByClass<Transform>());
		Check(document->ExecuteCommand(std::make_unique<RenameActorCommand>(
			scene, root->GetGuid(), "SavedRoot")) && document->IsDirty(),
			"An editing command marks the ActorImprint document dirty before save");
		const TagId savedTag = TagRegistry::Get().Register("TagTest_Imprint");
		Check(document->ExecuteCommand(std::make_unique<ChangeActorTagCommand>(
			scene, root->GetGuid(), savedTag)) && root->GetTag() == savedTag,
			"ActorImprint document applies the shared Actor tag command");
		Check(document->Undo() && root->GetTag() == TAG_NONE &&
			document->Redo() && root->GetTag() == savedTag,
			"ActorImprint document preserves tag changes through Undo and Redo");
		Check(document->Save() && !document->IsDirty(),
			"Validated atomic save marks the document clean only after success");
		Check(!HasTransactionResidue(fixture.root),
			"Successful replacement removes its same-directory transaction files");

		ActorImprintAssetError diskError;
		auto diskDefinition = ActorImprintAssetDeserializer::Load(
			(fixture.root / "Editable.imprint").string(), nullptr, &diskError);
		Check(diskDefinition && diskDefinition->GetActors().front().properties["name"] == "SavedRoot" &&
			diskDefinition->GetActors().front().properties["tag"] == "TagTest_Imprint" &&
			diskDefinition->GetRootActorId() == rootId &&
			diskDefinition->GetActors().front().components.front().id == transformId &&
			diskDefinition->GetRevision() != originalRevision,
			"Saved file contains edited name and tag state, stable IDs, and a new content revision");
		if (!diskDefinition) return;
		const DefinitionRevision savedRevision = diskDefinition->GetRevision();

		const auto changes = fixture.assets.TakePendingChanges();
		Check(changes.size() == 1 && changes.front() == AssetChange{
			AssetChangeKind::Modified, AssetType::ActorImprint, fixture.assetGuid, "Editable.imprint" },
			"Successful save publishes one content-only Modified notification");
		if (changes.size() == 1)
		{
			auto reload = manager.ReloadActorImprint(fixture.system, changes.front());
			Check(reload.status == ActorImprintReloadStatus::Reloaded &&
				reload.previousRevision == originalRevision && reload.currentRevision == savedRevision &&
				reload.affectedSceneIndices.empty(),
				"ET-14 reload commits the saved definition without treating the Working Scene as an Instance");
			reload.FinalizeRetiredScenes();
		}

		Check(document->Save(), "Equivalent content can be saved again");
		auto equivalent = ActorImprintAssetDeserializer::Load(
			(fixture.root / "Editable.imprint").string(), nullptr, &diskError);
		Check(equivalent && equivalent->GetRevision() == savedRevision &&
			document->GetEditingContext()->GetObjectMap().FindActor(root->GetGuid()) == rootId &&
			document->GetEditingContext()->GetObjectMap().FindComponent(
				root->GetComponentByClass<Transform>()) == transformId,
			"Equivalent saves retain DefinitionRevision and LocalObjectIDs");
	}

	void AtomicSaveFailurePreservesState()
	{
		Fixture fixture;
		auto context = fixture.Open();
		if (!context) { Check(false, "Save failure fixture opens"); return; }
		auto document = std::make_unique<ActorImprintEditorDocument>(std::move(context), 800, 600);
		fixture.assets.TakePendingChanges();
		const std::filesystem::path path = fixture.root / "Editable.imprint";
		const std::string originalBytes = ReadText(path);
		const ActorImprintHandle handle = fixture.system.FindHandle(fixture.assetGuid);
		const DefinitionRevision originalRevision = fixture.system.Resolve(handle)->GetRevision();
		Actor* root = document->GetWorkingScene()->GetRootActors().front();
		Check(document->ExecuteCommand(std::make_unique<RenameActorCommand>(
			document->GetWorkingScene(), root->GetGuid(), "BlockedSave")),
			"Save failure fixture creates a dirty edit");

		const HANDLE lock = CreateFileW(path.c_str(), GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		Check(lock != INVALID_HANDLE_VALUE, "Save failure fixture locks destination replacement");
		ActorImprintEditingSaveError saveError;
		const bool saved = document->GetEditingContext()->Save(&saveError);
		if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
		Check(!saved && saveError.code == ActorImprintEditingSaveErrorCode::AtomicReplaceFailed,
			"Destination replacement failure is reported as an atomic save failure");
		Check(document->IsDirty() && ReadText(path) == originalBytes &&
			fixture.system.Resolve(handle)->GetRevision() == originalRevision &&
			fixture.assets.TakePendingChanges().empty(),
			"Failed save preserves dirty state, original bytes, loaded definition, and notification queue");
		Check(!HasTransactionResidue(fixture.root),
			"Failed replacement recovers and removes temporary transaction files");
	}

	void PreparedSaveRollbackPreservesAuthoringState()
	{
		Fixture fixture;
		auto context = fixture.Open();
		if (!context) { Check(false, "Prepared save fixture opens"); return; }
		auto document = std::make_unique<ActorImprintEditorDocument>(std::move(context), 800, 600);
		fixture.assets.TakePendingChanges();
		const auto path = fixture.root / "Editable.imprint";
		const std::string originalBytes = ReadText(path);
		Actor* root = document->GetWorkingScene()->GetRootActors().front();
		Check(document->ExecuteCommand(std::make_unique<RenameActorCommand>(
			document->GetWorkingScene(), root->GetGuid(), "RollbackRoot")) && document->IsDirty(),
			"Prepared save rollback fixture creates a dirty authoring change");
		Check(document->PrepareSave() && document->GetEditingContext()->HasPendingSave() &&
			document->IsDirty() && ReadText(path) != originalBytes,
			"Prepared ActorImprint save stays dirty until the reload boundary commits");
		fixture.assets.TakePendingChanges();
		Check(document->GetEditingContext()->RollbackPendingSave() &&
			!document->GetEditingContext()->HasPendingSave() && document->IsDirty() &&
			ReadText(path) == originalBytes,
			"Reload failure rollback restores the original Asset while preserving the dirty Working Scene");
		Check(document->Save() && !document->IsDirty(),
			"A rolled-back prepared save can be retried through the normal Document Save path");
	}

	void ProjectTagManagement()
	{
		Fixture fixture;
		ProjectSettings settings;
		EditorDocumentManager documents;
		const std::string settingsPath = (fixture.root / "project.101").string();
		Check(static_cast<bool>(TagManagementWorkflow::Create(" WorkflowUnused ", settings, settingsPath)),
			"Project Tag workflow creates and normalizes a Tag");
		Check(settings.GetUserTags() == std::vector<std::string>{"WorkflowUnused"} &&
			TagRegistry::Get().ContainsName("WorkflowUnused"),
			"Create commits ProjectSettings and the shared Registry");
		Check(static_cast<bool>(TagManagementWorkflow::Rename("WorkflowUnused", "WorkflowRenamed", settings,
			settingsPath, fixture.assets, documents)),
			"Project Tag workflow renames an unused Tag");
		Check(!TagRegistry::Get().ContainsName("WorkflowUnused") &&
			TagRegistry::Get().ContainsName("WorkflowRenamed"),
			"Rename replaces the Registry entry");
		Check(static_cast<bool>(TagManagementWorkflow::Delete("WorkflowRenamed", settings, settingsPath,
			fixture.assets, documents)) && settings.GetUserTags().empty(),
			"Project Tag workflow deletes an unused Tag");

		Check(static_cast<bool>(TagManagementWorkflow::Create("WorkflowUsed", settings, settingsPath)),
			"Usage fixture creates a project Tag");
		auto context = fixture.Open();
		auto owner = std::make_unique<ActorImprintEditorDocument>(std::move(context), 800, 600);
		Actor* root = owner->GetWorkingScene()->GetRootActors().front();
		root->SetTag(TagRegistry::Get().GetId("WorkflowUsed"));
		documents.AddDocument(std::move(owner));
		const TagManagementResult blocked = TagManagementWorkflow::Delete("WorkflowUsed", settings,
			settingsPath, fixture.assets, documents);
		Check(!blocked && !blocked.usages.empty() &&
			TagRegistry::Get().ContainsName("WorkflowUsed"),
			"Delete rejects a Tag used by an open Working Scene");
		documents.Clear();
		TagRegistry::Get().UnregisterUserTag("WorkflowUsed");

		Check(static_cast<bool>(TagManagementWorkflow::Create("DiskUsed", settings, settingsPath)),
			"Saved usage fixture creates a project Tag");
		std::string stored = ReadText(fixture.root / "Editable.imprint");
		const std::size_t none = stored.find("\"tag\": \"None\"");
		if (none != std::string::npos) stored.replace(none, std::string("\"tag\": \"None\"").size(), "\"tag\": \"DiskUsed\"");
		{
			std::ofstream output(fixture.root / "Editable.imprint", std::ios::binary | std::ios::trunc);
			output << stored;
		}
		const TagManagementResult diskBlocked = TagManagementWorkflow::Rename("DiskUsed", "DiskRenamed",
			settings, settingsPath, fixture.assets, documents);
		Check(!diskBlocked && !diskBlocked.usages.empty() &&
			TagRegistry::Get().ContainsName("DiskUsed"),
			"Rename rejects a Tag used by a saved ActorImprint");
		TagRegistry::Get().UnregisterUserTag("DiskUsed");
	}
}

int main()
{
	OpenAndPolicy();
	CommandIdentityAndSnapshot();
	DocumentIsolationAndDeduplication();
	DefaultRootModel();
	SaveAndReload();
	AtomicSaveFailurePreservesState();
	PreparedSaveRollbackPreservesAuthoringState();
	ProjectTagManagement();
	return failures ? 1 : 0;
}
