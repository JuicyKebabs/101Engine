#include "Command/ComponentPropertyEditCommand.h"
#include "Command/EditorCommandHistory.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include <iostream>

namespace
{
	int failures = 0;
	void Check(bool value, const char* name)
	{
		(value ? std::cout : std::cerr) << (value ? "[PASS] " : "[FAIL] ") << name << '\n';
		if (!value) ++failures;
	}

	class TestComponent : public Component
	{
	public:
		int value = 0;
	private:
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	void TestExecuteUndoRedo()
	{
		TypeMetadataBuilder<TestComponent> builder("PropertyCommandTest");
		builder.AddMember("value", &TestComponent::value);
		auto metadata = builder.Build();
		ComponentRegistry::Get().Register(
			"PropertyCommandTest", [] { return static_cast<Component*>(new TestComponent); },
			typeid(TestComponent), ComponentCardinality::Multiple, ComponentFamily::None,
			std::make_unique<TypeMetadata>(std::move(*metadata)));

		SceneBase scene;
		Actor* actor = scene.AddRootActor(ActorFactory::CreateEmptyActor(Actor::InitDesc{}));
		auto* first = actor->AddComponent<TestComponent>();
		auto* second = actor->AddComponent<TestComponent>();
		first->value = 1;
		second->value = 2;
		const auto path = PropertyPath::FromString("/value");
		EditorCommandHistory history;
		ComponentPropertyIdentity identity{ actor->GetGuid(), typeid(TestComponent), 1, *path };

		Check(history.Execute(std::make_unique<ComponentPropertyEditCommand>(
			&scene, identity, std::int64_t{ 2 }, std::int64_t{ 8 })) && second->value == 8 && first->value == 1,
			"Execute resolves the exact component occurrence and property path");
		Check(history.Undo() && second->value == 2, "Undo restores the previous value");
		Check(history.Redo() && second->value == 8, "Redo re-resolves and reapplies the value");

		const auto missing = PropertyPath::FromString("/missing");
		ComponentPropertyIdentity missingIdentity{ actor->GetGuid(), typeid(TestComponent), 0, *missing };
		Check(!history.Execute(std::make_unique<ComponentPropertyEditCommand>(
			&scene, missingIdentity, std::int64_t{ 1 }, std::int64_t{ 3 })),
			"Missing property identity fails without entering command history");
	}
}

int main()
{
	TestExecuteUndoRedo();
	return failures == 0 ? 0 : 1;
}
