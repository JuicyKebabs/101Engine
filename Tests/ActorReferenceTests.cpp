#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorReference.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/SceneBase.h"

#include <iostream>
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

	std::unique_ptr<Actor> MakeActor(const char* name)
	{
		return ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, name));
	}

	void TestEmptySetAndClear()
	{
		SceneBase scene;
		ActorReference reference;

		Check(!reference.HasValue() && reference.Resolve(scene) == nullptr,
			"Default ActorReference is empty");

		Actor* actor = scene.AddRootActor(MakeActor("Actor"));
		Check(reference.Set(actor) && reference.HasValue(),
			"Set stores a valid Actor reference");
		Check(reference.Resolve(scene) == actor,
			"ActorReference resolves its assigned Actor");

		Check(reference.Set(nullptr) && !reference.HasValue(),
			"Set nullptr clears the Actor reference");
		Check(reference.Resolve(scene) == nullptr,
			"Cleared ActorReference resolves to nullptr");
	}

	void TestPendingDestroyAndGuidReconnection()
	{
		SceneBase scene;
		Actor* original = scene.AddRootActor(MakeActor("Original"));
		const Guid actorId = original->GetGuid();
		const ActorHandle originalHandle = original->GetHandle();

		ActorReference reference;
		reference.Set(original);

		scene.RemoveActor(original);
		Check(reference.Resolve(scene) == nullptr,
			"ActorReference rejects an Actor pending destruction");
		Check(reference.HasValue() && reference.GetGuid() == actorId,
			"Missing Actor preserves the persistent Guid");

		scene.LateUpdate(0.0f);
		Check(reference.Resolve(scene) == nullptr,
			"ActorReference remains unresolved after garbage collection");

		Actor* restored = scene.AddRootActor(
			ActorFactory::RestoreEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Restored"),
				actorId));

		Check(restored && restored->GetHandle().index == originalHandle.index &&
			restored->GetHandle().generation == originalHandle.generation + 1,
			"Restored Actor receives a new handle generation");
		Check(reference.Resolve(scene) == restored,
			"ActorReference reconnects to an Actor restored with the same Guid");
	}

	void TestGuidOnlyReference()
	{
		SceneBase scene;
		const Guid actorId = GuidGenerator::Generate();
		ActorReference reference;

		Check(reference.SetGuid(actorId) && reference.Resolve(scene) == nullptr,
			"Guid-only ActorReference can remain pending");

		Actor* actor = scene.AddRootActor(
			ActorFactory::RestoreEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Later"),
				actorId));

		Check(reference.Resolve(scene) == actor,
			"Guid-only ActorReference resolves after the Actor is registered");

		Check(!reference.SetGuid(Guid{}) && reference.GetGuid() == actorId,
			"Invalid SetGuid input preserves the existing reference");
	}

	void TestCachedHandleCannotCrossScenes()
	{
		SceneBase sourceScene;
		SceneBase otherScene;
		Actor* source = sourceScene.AddRootActor(MakeActor("Source"));
		Actor* other = otherScene.AddRootActor(MakeActor("Other"));

		Check(source->GetHandle() == other->GetHandle(),
			"Independent Scenes can issue identical runtime handles");

		ActorReference reference;
		reference.Set(source);

		Check(reference.Resolve(otherScene) == nullptr,
			"ActorReference does not confuse equal handles from different Scenes");
		Check(reference.Resolve(sourceScene) == source,
			"ActorReference reacquires the correct Actor after a cross-Scene lookup");
	}
}

int main()
{
	TestEmptySetAndClear();
	TestPendingDestroyAndGuidReconnection();
	TestGuidOnlyReference();
	TestCachedHandleCannotCrossScenes();

	if (g_failures == 0)
	{
		std::cout << "All ActorReference tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " ActorReference test(s) failed.\n";
	return 1;
}
