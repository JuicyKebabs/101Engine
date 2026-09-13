#include "Command/ChangeActorTagCommand.h"
#include "Command/EditorCommandHistory.h"

#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/SceneBase.h"

#include <iostream>
#include <memory>
#include <string>

namespace
{
	int failures = 0;

	void Check(bool condition, const char* message)
	{
		if (condition) { std::cout << "[PASS] " << message << '\n'; return; }
		std::cerr << "[FAIL] " << message << '\n';
		++failures;
	}

	Actor* AddActor(SceneBase& scene, TagId tag = TAG_NONE)
	{
		return scene.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, tag, "TaggedActor")));
	}

	void TestRegistryPresentationSnapshot()
	{
		TagRegistry& registry = TagRegistry::Get();
		const TagId zebra = registry.Register("TagTest_Zebra");
		const TagId alpha = registry.Register("TagTest_Alpha");
		const auto tags = registry.GetRegisteredTags();
		Check(!tags.empty() && tags.front().first == TAG_NONE && tags.front().second == "None",
			"Tag list places None first");
		auto alphaIt = std::find_if(tags.begin(), tags.end(), [alpha](const auto& entry) { return entry.first == alpha; });
		auto zebraIt = std::find_if(tags.begin(), tags.end(), [zebra](const auto& entry) { return entry.first == zebra; });
		Check(alphaIt != tags.end() && zebraIt != tags.end() && alphaIt < zebraIt,
			"Registered tags are listed deterministically by name");
		std::vector<std::string> normalized;
		std::string error;
		Check(!TagRegistry::ValidateUserTagSet({"None"}, normalized, &error),
			"Project Tag validation rejects reserved names");
		Check(!TagRegistry::ValidateUserTagSet({"costarring", "liquid"}, normalized, &error),
			"Project Tag validation rejects FNV-1a collisions");
		Check(!registry.UnregisterUserTag("MainCamera"),
			"Reserved Tags cannot be unregistered");
	}

	void TestChangeUndoRedoAndNone()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddActor(scene);
		const TagId player = TagRegistry::Get().Register("TagTest_Player");
		Check(history.Execute(std::make_unique<ChangeActorTagCommand>(
			&scene, actor->GetGuid(), player)) && actor->GetTag() == player,
			"Tag command changes None to a registered tag");
		Check(history.Undo() && actor->GetTag() == TAG_NONE,
			"Undo restores the previous tag");
		Check(history.Redo() && actor->GetTag() == player,
			"Redo restores the changed tag");
		Check(history.Execute(std::make_unique<ChangeActorTagCommand>(
			&scene, actor->GetGuid(), TAG_NONE)) && actor->GetTag() == TAG_NONE,
			"Tag command changes a registered tag to None");
	}

	void TestFailuresPreserveStateAndHistory()
	{
		SceneBase scene;
		EditorCommandHistory history;
		Actor* actor = AddActor(scene, ActorTags::MainCamera);
		Check(!history.Execute(std::make_unique<ChangeActorTagCommand>(
			&scene, actor->GetGuid(), ActorTags::MainCamera)),
			"Unchanged tag is rejected");
		Check(!history.Execute(std::make_unique<ChangeActorTagCommand>(
			nullptr, actor->GetGuid(), TAG_NONE)),
			"Null Scene is rejected");
		Check(!history.Execute(std::make_unique<ChangeActorTagCommand>(
			&scene, GuidGenerator::Generate(), TAG_NONE)),
			"Missing Actor is rejected");
		Check(history.GetUndoCount() == 0 && actor->GetTag() == ActorTags::MainCamera,
			"Rejected commands preserve Actor state and history");

		const TagId enemy = TagRegistry::Get().Register("TagTest_Enemy");
		Check(history.Execute(std::make_unique<ChangeActorTagCommand>(
			&scene, actor->GetGuid(), enemy)), "Valid tag change enters history");
		actor->SetTag(TAG_NONE);
		Check(!history.Undo() && actor->GetTag() == TAG_NONE && history.GetUndoCount() == 1,
			"Unexpected external mutation makes Undo fail without advancing history");
	}
}

int main()
{
	TestRegistryPresentationSnapshot();
	TestChangeUndoRedoAndNone();
	TestFailuresPreserveStateAndHistory();
	if (failures == 0) { std::cout << "All Actor tag editing tests passed.\n"; return 0; }
	std::cerr << failures << " Actor tag editing test(s) failed.\n";
	return 1;
}
