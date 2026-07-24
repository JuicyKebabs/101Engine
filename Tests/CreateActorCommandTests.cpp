#include "Command/CreateActorCommand.h"
#include "Command/EditorCommandHistory.h"

#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/SceneBase.h"

#include <iostream>
#include <memory>
#include <string>

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

	void TestCreateUndoRedoPreservesGuid()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor::InitDesc desc(true, TAG_NONE, "CommandActor");
		auto command = std::make_unique<CreateActorCommand>(&scene, desc);
		CreateActorCommand* commandView = command.get();

		Check(history.Execute(std::move(command)),
			"CreateActorCommand creates an actor through history");
		const Guid persistedGuid = commandView->GetActorGuid();
		Actor* created = scene.ResolveActor(persistedGuid);
		Check(persistedGuid.IsValid(), "Initial Execute stores a valid Guid");
		Check(created != nullptr && created->GetName() == "CommandActor",
			"Initial Execute registers the requested actor");
		Check(created && created->HasComponent<Transform>(),
			"Created actor retains the default Transform component");

		Check(history.Undo(), "Undo marks the created actor for destruction");
		Check(created->IsDestroyed(), "Undo marks the resolved actor as destroyed");
		Check(scene.ResolveActor(persistedGuid) == created,
			"Guid remains resolvable until deferred garbage collection");

		Check(!history.Redo(),
			"Redo safely fails while the old Guid is pending collection");
		Check(history.GetUndoCount() == 0 && history.GetRedoCount() == 1,
			"Failed early Redo remains available on the redo stack");

		scene.LateUpdate(0.0f);
		Check(scene.ResolveActor(persistedGuid) == nullptr,
			"LateUpdate removes the undone actor and its Guid mapping");
		Check(history.Redo(), "Redo restores the actor after garbage collection");

		Actor* restored = scene.ResolveActor(persistedGuid);
		Check(restored != nullptr, "Redo restores the original Guid");
		Check(restored && restored->GetGuid() == persistedGuid,
			"Redo does not generate a replacement Guid");
		Check(restored && restored->GetName() == "CommandActor",
			"Redo restores the original initialization data");
	}

	void TestRepeatedUndoRedo()
	{
		SceneBase scene;
		EditorCommandHistory history;
		auto command = std::make_unique<CreateActorCommand>(
			&scene,
			Actor::InitDesc(true, TAG_NONE, "Repeated"));
		CreateActorCommand* commandView = command.get();
		history.Execute(std::move(command));
		const Guid guid = commandView->GetActorGuid();

		for (int cycle = 0; cycle < 2; ++cycle)
		{
			Check(history.Undo(), "Repeated Undo succeeds");
			scene.LateUpdate(0.0f);
			Check(history.Redo(), "Repeated Redo succeeds");
			Check(scene.ResolveActor(guid) != nullptr,
				"Repeated Redo continues to preserve the Guid");
		}
	}

	void TestInvalidUsage()
	{
		EditorCommandHistory history;
		Check(!history.Execute(std::make_unique<CreateActorCommand>(
			nullptr,
			Actor::InitDesc(true, TAG_NONE, "Invalid"))),
			"CreateActorCommand rejects a null Scene");

		SceneBase scene;
		CreateActorCommand command(
			&scene,
			Actor::InitDesc(true, TAG_NONE, "NotExecuted"));
		Check(!command.Undo(), "Undo fails before the first Execute");
		Check(scene.GetAllActors().empty(),
			"Invalid command usage does not modify the Scene");
	}
}

int main()
{
	TestCreateUndoRedoPreservesGuid();
	TestRepeatedUndoRedo();
	TestInvalidUsage();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " CreateActorCommand test(s) failed.\n";
		return 1;
	}

	std::cout << "All CreateActorCommand tests passed.\n";
	return 0;
}
