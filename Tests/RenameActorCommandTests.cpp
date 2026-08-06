#include "Command/EditorCommandHistory.h"
#include "Command/RenameActorCommand.h"

#include "Engine/Actor/ActorFactory.h"
#include "Engine/Core/GUID/GuidGenerator.h"
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

	Actor* AddRoot(SceneBase& scene, const char* name)
	{
		return scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, name)));
	}

	void TestRenameUndoRedo()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddRoot(scene, "Before");
		const Guid actorGuid = actor->GetGuid();

		Check(history.Execute(
			std::make_unique<RenameActorCommand>(
				&scene,
				actorGuid,
				"After")),
			"RenameActorCommand renames an Actor by Guid");
		Check(actor->GetName() == "After" && actor->GetGuid() == actorGuid,
			"Rename changes only the Actor name");

		Check(history.Undo() && actor->GetName() == "Before",
			"Undo restores the original Actor name");
		Check(history.Redo() && actor->GetName() == "After",
			"Redo restores the renamed Actor name");
	}

	void TestInvalidRenameDoesNotEnterHistory()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddRoot(scene, "Actor");

		Check(!history.Execute(
			std::make_unique<RenameActorCommand>(
				&scene,
				actor->GetGuid(),
				"")),
			"RenameActorCommand rejects an empty name");
		Check(!history.Execute(
			std::make_unique<RenameActorCommand>(
				&scene,
				actor->GetGuid(),
				"Actor")),
			"RenameActorCommand rejects an unchanged name");
		Check(!history.Execute(
			std::make_unique<RenameActorCommand>(
				&scene,
				GuidGenerator::Generate(),
				"Missing")),
			"RenameActorCommand rejects an unresolved Actor Guid");
		Check(history.GetUndoCount() == 0,
			"Rejected Rename commands do not enter history");
	}

	void TestUnexpectedExternalRenameFailsSafely()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddRoot(scene, "Before");

		Check(history.Execute(
			std::make_unique<RenameActorCommand>(
				&scene,
				actor->GetGuid(),
				"After")),
			"Rename executes before testing an unexpected state");

		actor->SetName("External");

		Check(!history.Undo(),
			"Undo rejects an Actor name changed outside the command");
		Check(actor->GetName() == "External" &&
			history.GetUndoCount() == 1,
			"Failed Undo preserves the external name and command history");
	}
}

int main()
{
	TestRenameUndoRedo();
	TestInvalidRenameDoesNotEnterHistory();
	TestUnexpectedExternalRenameFailsSafely();

	if (g_failures == 0)
	{
		std::cout << "All RenameActorCommand tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " RenameActorCommand test(s) failed.\n";
	return 1;
}
