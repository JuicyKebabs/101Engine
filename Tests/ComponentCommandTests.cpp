#include "Command/AddComponentCommand.h"
#include "Command/EditorCommandHistory.h"
#include "Command/RemoveComponentCommand.h"

#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"

#include <iostream>
#include <memory>
#include <string>
#include <typeindex>

namespace
{
	int g_failures = 0;

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

	class CommandTestComponent : public Component
	{
	private:
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	class FailingReferenceComponent final : public CommandTestComponent
	{
	public:
		bool ResolveReferences(SceneBase&) override { return false; }
	};

	template<class T>
	void RegisterTestComponent(const std::string& name)
	{
		ComponentRegistry::Get().Register(
			name,
			[]() { return static_cast<Component*>(new T()); },
			std::type_index(typeid(T)),
			ComponentCardinality::Multiple,
			ComponentFamily::None
		);
	}

	Actor* AddEmptyActor(SceneBase& scene, const std::string& name)
	{
		return scene.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, name)));
	}

	void TestAddUndoRedoPreservesEditedState()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddEmptyActor(scene, "AddActor");
		const std::type_index typeId = typeid(CommandTestComponent);

		Check(history.Execute(std::make_unique<AddComponentCommand>(
			&scene, actor->GetGuid(), "CommandTestComponent")),
			"AddComponentCommand adds a component through command history");

		Component* added = actor->GetComponentByExactType(typeId, 0);
		Check(added != nullptr,
			"Added component can be resolved by exact type and occurrence index");

		if (added) added->SetName("EditedAfterAdd");

		Check(history.Undo(),
			"Undo removes the component added by AddComponentCommand");
		Check(actor->GetComponentsByExactType(typeId).empty(),
			"Undo removes the added component immediately");
		Check(history.Redo(),
			"Redo restores the component added by AddComponentCommand");

		Component* restored = actor->GetComponentByExactType(typeId, 0);
		Check(restored && restored->GetName() == "EditedAfterAdd",
			"Redo restores the component state captured during Undo");
	}

	void TestRemoveUndoRedoRestoresStateAndPosition()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddEmptyActor(scene, "RemoveActor");
		const std::type_index typeId = typeid(CommandTestComponent);

		auto first = std::make_unique<CommandTestComponent>();
		first->SetName("First");
		auto second = std::make_unique<CommandTestComponent>();
		second->SetName("Second");

		Check(scene.AddActorComponentImmediate(actor, std::move(first), 0) != nullptr &&
			scene.AddActorComponentImmediate(actor, std::move(second), 1) != nullptr,
			"Remove command test creates two same-type components");

		Check(history.Execute(std::make_unique<RemoveComponentCommand>(
			&scene, actor->GetGuid(), "CommandTestComponent", 0)),
			"RemoveComponentCommand removes the selected occurrence");
		Check(actor->GetComponentsByExactType(typeId).size() == 1 &&
			actor->GetComponentByExactType(typeId, 0)->GetName() == "Second",
			"Remove leaves the other same-type component in place");

		Check(history.Undo(),
			"Undo restores a removed component");
		Check(actor->GetComponentsByExactType(typeId).size() == 2 &&
			actor->GetComponentByExactType(typeId, 0)->GetName() == "First" &&
			actor->GetComponentByExactType(typeId, 1)->GetName() == "Second",
			"Undo restores component data and exact-type occurrence position");

		Check(history.Redo(),
			"Redo removes the restored component again");
		Check(actor->GetComponentsByExactType(typeId).size() == 1 &&
			actor->GetComponentByExactType(typeId, 0)->GetName() == "Second",
			"Redo targets the restored occurrence consistently");
	}

	void TestInvalidCommandsDoNotEnterHistory()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddEmptyActor(scene, "ValidationActor");

		Check(!history.Execute(std::make_unique<RemoveComponentCommand>(
			&scene, actor->GetGuid(), "Transform", 0)),
			"RemoveComponentCommand rejects a UniqueRequired Transform");
		Check(history.GetUndoCount() == 0,
			"Failed Transform removal is not added to command history");

		Check(actor->AddComponent<MeshRenderer>() != nullptr,
			"Validation test adds an initial Renderer-family component");
		Check(!history.Execute(std::make_unique<AddComponentCommand>(
			&scene, actor->GetGuid(), "SpriteRenderer")),
			"AddComponentCommand rejects a Renderer-family conflict");
		Check(history.GetUndoCount() == 0 &&
			actor->GetComponentByClass<SpriteRenderer>() == nullptr,
			"Rejected Renderer addition leaves history and Actor unchanged");
	}

	void TestRedoReferenceFailureRollsBackRestoredComponent()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddEmptyActor(scene, "ReferenceFailureActor");
		const std::type_index typeId = typeid(FailingReferenceComponent);

		Check(history.Execute(std::make_unique<AddComponentCommand>(
			&scene, actor->GetGuid(), "FailingReferenceComponent")),
			"Add command initially creates a component without persisted references");
		Check(history.Undo(),
			"Undo captures and removes the failing-reference component");
		Check(!history.Redo(),
			"Redo reports reference-resolution failure");
		Check(actor->GetComponentsByExactType(typeId).empty(),
			"Failed Redo rolls back the partially restored component");
		Check(history.GetUndoCount() == 0 && history.GetRedoCount() == 1,
			"Failed Redo leaves the command on the redo stack");
	}
}

int main()
{
	RegisterTestComponent<CommandTestComponent>("CommandTestComponent");
	RegisterTestComponent<FailingReferenceComponent>("FailingReferenceComponent");

	TestAddUndoRedoPreservesEditedState();
	TestRemoveUndoRedoRestoresStateAndPosition();
	TestInvalidCommandsDoNotEnterHistory();
	TestRedoReferenceFailureRollsBackRestoredComponent();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " Component command test(s) failed.\n";
		return 1;
	}

	std::cout << "All Component command tests passed.\n";
	return 0;
}
