#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Component.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/ComponentSnapshot.h"
#include "Engine/Scene/SceneBase.h"

#include <iostream>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

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

	class SnapshotTestComponent final : public Component
	{
	private:
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	void RegisterSnapshotTestComponent()
	{
		ComponentRegistry::Get().Register(
			"SnapshotTestComponent",
			[]() { return static_cast<Component*>(new SnapshotTestComponent()); },
			std::type_index(typeid(SnapshotTestComponent)),
			ComponentCardinality::Multiple,
			ComponentFamily::None
		);
	}

	SnapshotTestComponent* AddSnapshotTestComponent(
		SceneBase& scene,
		Actor& actor,
		const std::string& name,
		std::size_t occurrenceIndex)
	{
		auto component = std::make_unique<SnapshotTestComponent>();
		component->SetName(name);

		return dynamic_cast<SnapshotTestComponent*>(
			scene.AddActorComponentImmediate(
				&actor,
				std::move(component),
				occurrenceIndex
			)
		);
	}

	void TestCaptureAndRestorePreserveStateAndExactTypeIndex()
	{
		SceneBase scene;
		Actor* actor = scene.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "SnapshotActor")));

		SnapshotTestComponent* first =
			AddSnapshotTestComponent(scene, *actor, "First", 0);
		SnapshotTestComponent* second =
			AddSnapshotTestComponent(scene, *actor, "Second", 1);
		SnapshotTestComponent* third =
			AddSnapshotTestComponent(scene, *actor, "Third", 2);

		ComponentSnapshot snapshot;
		const bool captured = snapshot.Capture(actor, second);

		Check(first && second && third,
			"Actor accepts multiple registered components of the same exact type");
		Check(captured && snapshot.IsValid(),
			"ComponentSnapshot captures an owned component");
		Check(actor->GetComponentByExactType(
			std::type_index(typeid(SnapshotTestComponent)), 1) == second,
			"Actor resolves a component by exact type and occurrence index");

		Check(scene.RemoveActorComponentImmediate(actor, second),
			"Captured component can be removed before restoration");

		Component* restored = snapshot.Restore(&scene);
		const std::vector<Component*> components =
			actor->GetComponentsByExactType(
				std::type_index(typeid(SnapshotTestComponent)));

		Check(restored != nullptr,
			"ComponentSnapshot restores its captured Component");
		Check(components.size() == 3 &&
			components[0] == first &&
			components[1] == restored &&
			components[2] == third,
			"ComponentSnapshot restores the exact-type occurrence position");
		Check(restored && restored->GetName() == "Second",
			"ComponentSnapshot restores serialized Component state without exposing it");
	}

	void TestCaptureRejectsInvalidTargetsAndResetsState()
	{
		auto owner = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Owner"));
		auto other = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "Other"));

		SnapshotTestComponent* component =
			dynamic_cast<SnapshotTestComponent*>(owner->AddComponent(
				std::make_unique<SnapshotTestComponent>()));

		ComponentSnapshot snapshot;
		Check(snapshot.Capture(owner.get(), component),
			"ComponentSnapshot establishes valid state before a failed capture");
		Check(!snapshot.Capture(other.get(), component) && !snapshot.IsValid(),
			"ComponentSnapshot rejects a component owned by another Actor and resets state");
		Check(!snapshot.Capture(nullptr, component) && !snapshot.IsValid(),
			"ComponentSnapshot rejects a null Actor");
		Check(!snapshot.Capture(owner.get(), nullptr) && !snapshot.IsValid(),
			"ComponentSnapshot rejects a null Component");

		component->MarkForDestruction();
		Check(!snapshot.Capture(owner.get(), component) && !snapshot.IsValid(),
			"ComponentSnapshot rejects a destroyed Component");
	}
}

int main()
{
	RegisterSnapshotTestComponent();
	TestCaptureAndRestorePreserveStateAndExactTypeIndex();
	TestCaptureRejectsInvalidTargetsAndResetsState();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " ComponentSnapshot test(s) failed.\n";
		return 1;
	}

	std::cout << "All ComponentSnapshot tests passed.\n";
	return 0;
}
