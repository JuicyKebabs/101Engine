#include "Engine/Actor/ActorFactory.h"
#include "Engine/Actor/ActorPool.h"
#include "Engine/Scene/SceneBase.h"

#include <cstddef>
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

	std::unique_ptr<Actor> MakeActor(const std::string& name)
	{
		return ActorFactory::CreateEmptyActor(Actor::InitDesc(true, TAG_NONE, name));
	}

	void TestHandleLifetimeAndSlotReuse()
	{
		ActorPool pool;
		const ActorHandle first = pool.Register(MakeActor("First"));

		Check(!first.IsNull(), "Register returns a non-null handle");
		Check(pool.Resolve(first) != nullptr, "Registered handle resolves");

		pool.Destroy(first);
		Check(pool.Resolve(first) != nullptr,
			"Pending-destroy actor remains resolvable until garbage collection");

		pool.CollectGarbage();
		Check(pool.Resolve(first) == nullptr, "Collected handle no longer resolves");

		const ActorHandle replacement = pool.Register(MakeActor("Replacement"));
		Check(replacement.index == first.index, "Freed slot is reused");
		Check(replacement.generation == first.generation + 1,
			"Reused slot advances its generation");
		Check(pool.Resolve(first) == nullptr, "Old generation cannot resolve replacement actor");
		Check(pool.Resolve(replacement) != nullptr, "Replacement handle resolves");
	}

	void TestForEachWithHolesAndRegistration()
	{
		ActorPool pool;
		const ActorHandle first = pool.Register(MakeActor("First"));
		const ActorHandle second = pool.Register(MakeActor("Second"));
		pool.Destroy(first);
		pool.CollectGarbage();

		std::size_t visited = 0;
		pool.ForEach([&](Actor*) { ++visited; });
		Check(visited == 1 && pool.Resolve(second) != nullptr,
			"ForEach visits live slots after an earlier hole");

		ActorPool growingPool;
		growingPool.Register(MakeActor("Existing"));
		std::size_t callbacks = 0;
		growingPool.ForEach([&](Actor*) {
			++callbacks;
			growingPool.Register(MakeActor("AddedDuringTraversal"));
			});
		Check(callbacks == 1, "Actor appended during ForEach is deferred to the next traversal");

		visited = 0;
		growingPool.ForEach([&](Actor*) { ++visited; });
		Check(visited == 2, "Next ForEach sees the actor added during the previous traversal");
	}

	void TestHierarchyDestruction()
	{
		SceneBase scene;
		Actor* parent = scene.AddRootActor(MakeActor("Parent"));
		Actor* child = scene.AddChildActor(MakeActor("Child"), parent->GetHandle());
		const ActorHandle parentHandle = parent->GetHandle();
		const ActorHandle childHandle = child->GetHandle();

		parent->Destroy();
		Check(parent->IsDestroyed(), "Actor::Destroy marks the parent");
		Check(child->IsDestroyed(), "Actor::Destroy cascades to descendants");
		Check(scene.ResolveActor(parentHandle) != nullptr && scene.ResolveActor(childHandle) != nullptr,
			"Cascaded actors remain resolvable until collection");

		scene.LateUpdate(0.0f);
		Check(scene.ResolveActor(parentHandle) == nullptr, "Collected parent handle is invalid");
		Check(scene.ResolveActor(childHandle) == nullptr, "Collected child handle is invalid");
	}

	void TestNonCascadingDestruction()
	{
		SceneBase scene;
		Actor* parent = scene.AddRootActor(MakeActor("Parent"));
		Actor* child = scene.AddChildActor(MakeActor("Child"), parent->GetHandle());
		const ActorHandle parentHandle = parent->GetHandle();
		const ActorHandle childHandle = child->GetHandle();

		scene.RemoveActor(parent, false);
		Check(parent->IsDestroyed(), "Non-cascading removal marks the parent");
		Check(!child->IsDestroyed(), "Non-cascading removal preserves the child");
		Check(child->GetParentHandle().IsNull(), "Preserved child becomes a root actor");

		scene.LateUpdate(0.0f);
		Check(scene.ResolveActor(parentHandle) == nullptr, "Non-cascading parent is collected");
		Check(scene.ResolveActor(childHandle) == child, "Preserved child remains valid");
	}
}

int main()
{
	TestHandleLifetimeAndSlotReuse();
	TestForEachWithHolesAndRegistration();
	TestHierarchyDestruction();
	TestNonCascadingDestruction();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " ActorPool test(s) failed.\n";
		return 1;
	}

	std::cout << "All ActorPool tests passed.\n";
	return 0;
}
