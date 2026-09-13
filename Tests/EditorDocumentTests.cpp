#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "Document/EditorDocumentManager.h"
#include "Document/EditorDocumentWorkflow.h"
#include "Document/SceneEditorDocument.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Component/Camera.h"
#include "Engine/Scene/SceneBase.h"
#include "UI/DocumentTabBar.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace
{
	int failures = 0;

	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAILED: " << message << '\n';
		++failures;
	}

	class CounterCommand final : public IEditorCommand
	{
	public:
		explicit CounterCommand(int& value) : m_value(value) {}
		bool Execute() override { ++m_value; return true; }
		bool Undo() override { --m_value; return true; }

	private:
		int& m_value;
	};

	class AlternateDocument final : public IEditorDocument
	{
	public:
		explicit AlternateDocument(std::string name, int* externalSaveCount = nullptr)
			: IEditorDocument(640, 360)
			, m_scene(std::make_unique<SceneBase>())
			, m_name(std::move(name))
			, m_externalSaveCount(externalSaveCount)
		{
		}

		~AlternateDocument() override
		{
			if (m_scene) m_scene->Finalize();
		}

		SceneBase* GetWorkingScene() override { return m_scene.get(); }
		const SceneBase* GetWorkingScene() const override { return m_scene.get(); }
		bool Save() override
		{
			++saveCount;
			if (m_externalSaveCount) ++*m_externalSaveCount;
			if (!saveSucceeds) return false;
			MarkClean();
			return true;
		}
		bool CanEnterPlay() const override { return false; }
		EditorDocumentType GetType() const override { return EditorDocumentType::ActorImprint; }
		std::string_view GetDisplayName() const override { return m_name; }
		std::unique_ptr<SceneBase> ReplaceSceneForTest(std::unique_ptr<SceneBase> scene)
		{
			auto previous = std::move(m_scene);
			m_scene = std::move(scene);
			return previous;
		}

		int saveCount = 0;
		bool saveSucceeds = true;

	private:
		std::unique_ptr<SceneBase>* GetWorkingSceneOwnerSlot() override { return &m_scene; }
		std::unique_ptr<SceneBase> m_scene;
		std::string m_name;
		int* m_externalSaveCount = nullptr;
	};

	Guid TestGuid(const char* value)
	{
		return Guid::FromString(value);
	}

	std::unique_ptr<SceneBase> MakeSavableScene()
	{
		auto scene = std::make_unique<SceneBase>();
		Actor::InitDesc desc;
		desc.name = "MainCamera";
		desc.tag = ActorTags::MainCamera;
		auto actor = ActorFactory::CreateActor(ActorType::Camera, desc);
		Camera* camera = actor ? actor->GetComponentByClass<Camera>() : nullptr;
		if (actor) scene->AddRootActor(std::move(actor));
		if (camera) scene->GetCameraSystem()->SetMainCamera(camera);
		return scene;
	}

	void TestSceneDocumentSaveAndDirtyState()
	{
		const std::filesystem::path path = "build/et15_scene_document_test.scene";
		std::error_code error;
		std::filesystem::remove(path, error);

		SceneEditorDocument document(
			MakeSavableScene(), path.string(), 1280, 720);
		Check(document.GetWorkingScene() != nullptr, "Scene Document owns its working Scene");
		Check(document.CanEnterPlay(), "Scene Document allows Play");
		Check(document.GetDisplayName() == "et15_scene_document_test.scene",
			"Scene Document exposes a path-derived display identity");

		int value = 0;
		Check(document.ExecuteCommand(std::make_unique<CounterCommand>(value)),
			"Document executes a command through its history");
		Check(value == 1 && document.IsDirty(),
			"successful Document command marks only that Document dirty");
		Check(document.Save(), "Scene Document saves through its Document boundary");
		Check(std::filesystem::exists(path) && !document.IsDirty(),
			"successful save publishes the Scene and marks the Document clean");

		std::filesystem::remove(path, error);
	}

	void TestActiveSwitchKeepsStateIsolated()
	{
		EditorDocumentManager manager;
		auto first = std::make_unique<AlternateDocument>("First");
		auto* firstDocument = first.get();
		const EditorDocumentId firstId = manager.AddDocument(std::move(first));

		const Guid firstSelection = TestGuid("{11111111-1111-1111-1111-111111111111}");
		firstDocument->GetSelection().SelectActor(firstSelection);
		firstDocument->GetViewportContext().SetViewMode(EditorViewportMode::Canvas);
		firstDocument->GetViewportContext().GetCanvasNavigation().zoom = 2.5f;
		firstDocument->GetViewportContext().GetSceneCamera().Fly({1.0f, 0.0f, 0.0f}, 3.0f);
		const Vector3 firstCameraPosition = firstDocument->GetViewportContext()
			.GetSceneCamera().GetCamera().GetCameraPose().position;
		int firstValue = 0;
		firstDocument->ExecuteCommand(std::make_unique<CounterCommand>(firstValue));

		int secondSaveCount = 0;
		auto second = std::make_unique<AlternateDocument>("Second", &secondSaveCount);
		auto* secondDocument = second.get();
		const EditorDocumentId secondId = manager.AddDocument(std::move(second));
		Check(manager.GetActiveDocument() == secondDocument,
			"newly activated Document supplies the active Panel context");
		Check(secondDocument->GetSelection().GetSelectedActorGuid() != firstSelection &&
			secondDocument->GetViewportContext().GetViewMode() == EditorViewportMode::Scene &&
			secondDocument->GetViewportContext().GetCanvasNavigation().zoom == 1.0f &&
			!secondDocument->GetViewportContext().GetSceneCamera().GetCamera()
				.GetCameraPose().position.NearEqual(firstCameraPosition) &&
			!secondDocument->IsDirty(),
			"selection, viewport, and dirty state do not leak to another Document");
		Check(!secondDocument->CanEnterPlay(),
			"a non-Scene Document can reject Play through the shared interface");

		const Guid secondSelection = TestGuid("{22222222-2222-2222-2222-222222222222}");
		secondDocument->GetSelection().SelectActor(secondSelection);
		secondDocument->MarkDirty();
		Check(manager.ActivateDocument(firstId) && manager.GetActiveDocument() == firstDocument,
			"Document manager switches the active editing unit");
		Check(firstDocument->GetSelection().GetSelectedActorGuid() == firstSelection &&
			firstDocument->GetViewportContext().GetViewMode() == EditorViewportMode::Canvas &&
			firstDocument->GetViewportContext().GetCanvasNavigation().zoom == 2.5f &&
			firstDocument->GetCommandHistory().GetUndoCount() == 1 &&
			firstDocument->IsDirty(),
			"switching back restores that Document's independent state");
		Check(secondDocument->GetSelection().GetSelectedActorGuid() == secondSelection,
			"inactive Document state remains retained");

		Check(manager.CloseDocument(secondId, EditorDocumentCloseDecision::Cancel) ==
			EditorDocumentCloseResult::Cancelled && manager.GetDocumentCount() == 2,
			"dirty Document close requires an explicit non-cancel decision");
		Check(manager.CloseDocument(secondId, EditorDocumentCloseDecision::Save) ==
			EditorDocumentCloseResult::Closed && secondSaveCount == 1,
			"dirty Document can save before close");

		auto failing = std::make_unique<AlternateDocument>("Failing");
		auto* failingDocument = failing.get();
		failingDocument->saveSucceeds = false;
		failingDocument->MarkDirty();
		const EditorDocumentId failingId = manager.AddDocument(std::move(failing));
		Check(manager.CloseDocument(failingId, EditorDocumentCloseDecision::Save) ==
			EditorDocumentCloseResult::SaveFailed && manager.GetDocumentCount() == 2,
			"failed save keeps a dirty Document open");
		Check(manager.CloseDocument(failingId, EditorDocumentCloseDecision::Discard) ==
			EditorDocumentCloseResult::Closed,
			"explicit discard closes a dirty Document after save failure");
	}

	void TestSceneReplacementReconcilesOnlyAffectedDocuments()
	{
		EditorDocumentManager manager;
		auto first = std::make_unique<AlternateDocument>("First");
		auto* firstDocument = first.get();
		manager.AddDocument(std::move(first), false);
		auto second = std::make_unique<AlternateDocument>("Second");
		auto* secondDocument = second.get();
		manager.AddDocument(std::move(second), true);

		int firstValue = 0;
		int secondValue = 0;
		firstDocument->ExecuteCommand(std::make_unique<CounterCommand>(firstValue));
		secondDocument->ExecuteCommand(std::make_unique<CounterCommand>(secondValue));
		firstDocument->MarkClean();
		firstDocument->GetSelection().SelectActor(
			TestGuid("{33333333-3333-3333-3333-333333333333}"));
		secondDocument->GetSelection().SelectActor(
			TestGuid("{44444444-4444-4444-4444-444444444444}"));

		auto retired = firstDocument->ReplaceSceneForTest(std::make_unique<SceneBase>());
		const std::size_t replacedIndex = 0;
		manager.ReconcileSceneReplacements(std::span<const std::size_t>(&replacedIndex, 1));

		Check(firstDocument->GetCommandHistory().GetUndoCount() == 0 &&
			!firstDocument->GetSelection().GetSelectedActorGuid().IsValid() &&
			firstDocument->IsDirty(),
			"replaced Document clears pointer-bearing history, revalidates selection, and becomes dirty");
		Check(secondDocument->GetCommandHistory().GetUndoCount() == 1 &&
			secondDocument->GetSelection().GetSelectedActorGuid().IsValid(),
			"unaffected Document state is preserved across another Scene replacement");
		if (retired) retired->Finalize();
	}

	void TestDocumentTabIntentUsesWorkflowBoundary()
	{
		EditorDocumentManager manager;
		EditorDocumentWorkflow workflow(manager);
		auto first = std::make_unique<AlternateDocument>("First");
		const EditorDocumentId firstId = manager.AddDocument(std::move(first));
		auto second = std::make_unique<AlternateDocument>("Second");
		const EditorDocumentId secondId = manager.AddDocument(std::move(second));

		DocumentTabBar::Callbacks callbacks;
		callbacks.onActivate = [&workflow](EditorDocumentId id) { workflow.Activate(id); };
		callbacks.onClose = [&workflow](EditorDocumentId id) { workflow.RequestClose(id); };
		Check(DocumentTabBar::DispatchActivate(callbacks, firstId) &&
			manager.GetActiveDocumentId() == firstId,
			"Document tab activation intent reaches the production workflow boundary");
		Check(DocumentTabBar::DispatchClose(callbacks, secondId) &&
			manager.FindDocument(secondId) == nullptr && manager.GetDocumentCount() == 1,
			"Document tab close intent closes a clean Document through the workflow boundary");

		const auto documents = manager.GetDocuments();
		Check(documents.size() == 1 && documents.front().id == firstId &&
			documents.front().displayName == "First" && documents.front().isActive &&
			!documents.front().isDirty,
			"Document enumeration exposes only stable UI state without ownership");
	}

	void TestDocumentTabModelSelectionConvergesAcrossFrames()
	{
		EditorDocumentManager manager;
		EditorDocumentWorkflow workflow(manager);
		auto first = std::make_unique<AlternateDocument>("First");
		const EditorDocumentId firstId = manager.AddDocument(std::move(first));

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(800.0f, 600.0f);
		io.DeltaTime = 1.0f / 60.0f;
		unsigned char* pixels = nullptr;
		int textureWidth = 0;
		int textureHeight = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &textureWidth, &textureHeight);

		DocumentTabBar tabBar;
		std::optional<EditorDocumentId> requestedActivation;
		int activationCount = 0;
		DocumentTabBar::Callbacks callbacks;
		callbacks.onActivate = [&](EditorDocumentId id)
		{
			requestedActivation = id;
			++activationCount;
		};

		auto renderFrame = [&]()
		{
			requestedActivation.reset();
			ImGui::NewFrame();
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f));
			ImGui::Begin("DocumentTabHost", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
			tabBar.Render(manager.GetDocuments(), callbacks);
			ImGui::End();
			ImGui::Render();
			if (requestedActivation) workflow.Activate(*requestedActivation);
		};

		renderFrame();
		renderFrame();
		Check(manager.GetActiveDocumentId() == firstId && activationCount == 0,
			"initial tab selection converges without an activation callback");

		auto second = std::make_unique<AlternateDocument>("Second");
		const EditorDocumentId secondId = manager.AddDocument(std::move(second));
		renderFrame();
		renderFrame();
		renderFrame();
		Check(manager.GetActiveDocumentId() == secondId && activationCount == 0,
			"programmatic activation suppresses the stale visible tab until ImGui converges");

		ImGuiWindow* hostWindow = ImGui::FindWindowByName("DocumentTabHost");
		ImGuiTabBar* tabBarState = hostWindow
			? GImGui->TabBars.GetByKey(hostWindow->GetID("##EditorDocuments")) : nullptr;
		Check(tabBarState && tabBarState->Tabs.Size == 2,
			"test locates both rendered Document tabs");
		if (tabBarState && tabBarState->Tabs.Size == 2)
		{
			const ImGuiTabItem& firstTab = tabBarState->Tabs[0];
			const ImVec2 firstTabCenter(
				tabBarState->BarRect.Min.x + firstTab.Offset + firstTab.Width * 0.5f,
				(tabBarState->BarRect.Min.y + tabBarState->BarRect.Max.y) * 0.5f);
			io.AddMousePosEvent(firstTabCenter.x, firstTabCenter.y);
			renderFrame();
			io.AddMouseButtonEvent(0, true);
			renderFrame();
			io.AddMouseButtonEvent(0, false);
			renderFrame();
			renderFrame();
		}
		Check(manager.GetActiveDocumentId() == firstId && activationCount == 1,
			"one user tab click activates the Scene Document exactly once");

		Check(manager.ActivateDocument(secondId),
			"test can programmatically reactivate the second Document");
		renderFrame();
		renderFrame();
		renderFrame();
		Check(manager.GetActiveDocumentId() == secondId && activationCount == 1,
			"external activation remains stable after multiple ImGui frames");

		Check(manager.CloseDocument(secondId, EditorDocumentCloseDecision::Discard) ==
			EditorDocumentCloseResult::Closed,
			"active Document closes and selects its fallback");
		renderFrame();
		renderFrame();
		Check(manager.GetActiveDocumentId() == firstId && activationCount == 1,
			"active close synchronizes the fallback tab without stale activation");

		Check(manager.CloseDocument(firstId, EditorDocumentCloseDecision::Discard) ==
			EditorDocumentCloseResult::Closed,
			"last Document closes");
		renderFrame();
		Check(!manager.GetActiveDocumentId().IsValid() && activationCount == 1,
			"zero Document tabs are safe and do not dispatch activation");

		ImGui::DestroyContext();
	}

	void TestExitWorkflowPreservesDocumentsUntilCommit()
	{
		EditorDocumentManager manager;
		EditorDocumentWorkflow workflow(manager);
		int firstSaveCount = 0;
		auto first = std::make_unique<AlternateDocument>("First", &firstSaveCount);
		auto* firstDocument = first.get();
		const EditorDocumentId firstId = manager.AddDocument(std::move(first));
		firstDocument->MarkDirty();

		auto second = std::make_unique<AlternateDocument>("Second");
		auto* secondDocument = second.get();
		const EditorDocumentId secondId = manager.AddDocument(std::move(second));
		secondDocument->MarkDirty();
		secondDocument->saveSucceeds = false;

		Check(workflow.RequestExit() == EditorDocumentWorkflowResult::ConfirmationRequired &&
			workflow.GetPendingDocumentId() == firstId,
			"Editor exit begins with the first dirty Document");
		Check(workflow.ResolvePending(EditorDocumentCloseDecision::Save) ==
			EditorDocumentWorkflowResult::ConfirmationRequired &&
			firstSaveCount == 1 && !firstDocument->IsDirty() &&
			workflow.GetPendingDocumentId() == secondId && manager.GetDocumentCount() == 2,
			"successful exit save retains every Document and advances to the next dirty one");
		Check(workflow.ResolvePending(EditorDocumentCloseDecision::Save) ==
			EditorDocumentWorkflowResult::SaveFailed && manager.GetDocumentCount() == 2 &&
			secondDocument->IsDirty(),
			"failed exit save preserves the dirty Document and pending decision");
		Check(workflow.ResolvePending(EditorDocumentCloseDecision::Cancel) ==
			EditorDocumentWorkflowResult::Cancelled && manager.GetDocumentCount() == 2 &&
			!workflow.IsExitPending(),
			"cancelled Editor exit retains all Documents and their state");

		Check(workflow.RequestExit() == EditorDocumentWorkflowResult::ConfirmationRequired &&
			workflow.GetPendingDocumentId() == secondId &&
			workflow.ResolvePending(EditorDocumentCloseDecision::Discard) ==
			EditorDocumentWorkflowResult::ExitReady && manager.GetDocumentCount() == 2,
			"Editor exit becomes ready only after every dirty Document is resolved");
		firstDocument->MarkDirty();
		Check(workflow.RequestExit() == EditorDocumentWorkflowResult::ConfirmationRequired &&
			workflow.GetPendingDocumentId() == firstId,
			"exit workflow rechecks Documents dirtied by save-side reload processing");
		Check(workflow.ResolvePending(EditorDocumentCloseDecision::Discard) ==
			EditorDocumentWorkflowResult::ExitReady,
			"rechecked Document can be resolved without closing it before exit commit");
		secondDocument->MarkDirty();
		Check(workflow.RequestExit() == EditorDocumentWorkflowResult::ConfirmationRequired &&
			workflow.GetPendingDocumentId() == secondId,
			"exit workflow rechecks a previously discarded Document when its dirty generation changes");
	}

	void TestApplicationSaveBoundaryGatesCloseAndExit()
	{
		EditorDocumentManager manager;
		EditorDocumentWorkflow workflow(manager);
		auto document = std::make_unique<AlternateDocument>("Prepared Save");
		auto* liveDocument = document.get();
		const EditorDocumentId documentId = manager.AddDocument(std::move(document));
		liveDocument->MarkDirty();

		int saveAttempts = 0;
		bool completeSave = false;
		workflow.SetSaveCallback([&](EditorDocumentId id)
		{
			++saveAttempts;
			if (id != documentId || !completeSave) return false;
			return manager.SaveDocument(id);
		});

		Check(workflow.RequestClose(documentId) ==
			EditorDocumentWorkflowResult::ConfirmationRequired,
			"dirty close waits for a save decision");
		Check(workflow.ResolvePending(EditorDocumentCloseDecision::Save) ==
			EditorDocumentWorkflowResult::SaveFailed && saveAttempts == 1 &&
			manager.FindDocument(documentId) == liveDocument && liveDocument->IsDirty(),
			"failed application save keeps the dirty Document open and pending");

		completeSave = true;
		Check(workflow.ResolvePending(EditorDocumentCloseDecision::Save) ==
			EditorDocumentWorkflowResult::Closed && saveAttempts == 2 &&
			manager.FindDocument(documentId) == nullptr,
			"close commits only after the application save boundary succeeds");

		auto exitDocument = std::make_unique<AlternateDocument>("Exit Save");
		auto* liveExitDocument = exitDocument.get();
		const EditorDocumentId exitDocumentId = manager.AddDocument(std::move(exitDocument));
		liveExitDocument->MarkDirty();
		completeSave = false;
		workflow.SetSaveCallback([&](EditorDocumentId id)
		{
			++saveAttempts;
			if (id != exitDocumentId || !completeSave) return false;
			return manager.SaveDocument(id);
		});

		Check(workflow.RequestExit() == EditorDocumentWorkflowResult::ConfirmationRequired,
			"dirty exit waits for the application save boundary");
		Check(workflow.ResolvePending(EditorDocumentCloseDecision::Save) ==
			EditorDocumentWorkflowResult::SaveFailed && workflow.IsExitPending() &&
			workflow.GetPendingDocumentId() == exitDocumentId && liveExitDocument->IsDirty(),
			"failed application save keeps exit pending and preserves dirty state");
		completeSave = true;
		Check(workflow.ResolvePending(EditorDocumentCloseDecision::Save) ==
			EditorDocumentWorkflowResult::ExitReady && !liveExitDocument->IsDirty(),
			"exit becomes ready only after the application save boundary succeeds");
	}
}

int main()
{
	TestSceneDocumentSaveAndDirtyState();
	TestActiveSwitchKeepsStateIsolated();
	TestSceneReplacementReconcilesOnlyAffectedDocuments();
	TestDocumentTabIntentUsesWorkflowBoundary();
	TestDocumentTabModelSelectionConvergesAcrossFrames();
	TestExitWorkflowPreservesDocumentsUntilCommit();
	TestApplicationSaveBoundaryGatesCloseAndExit();

	if (failures != 0) return EXIT_FAILURE;
	std::cout << "EditorDocumentTests passed\n";
	return EXIT_SUCCESS;
}
