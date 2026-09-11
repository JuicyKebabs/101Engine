#include "Command/ComponentPropertyEditCommand.h"
#include "Command/EditorCommandHistory.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Component/Transform.h"
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
		builder.Property("value", &TestComponent::value);
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

void TestTransformPilot()
{
	SceneBase scene;
	Actor* actor = scene.AddRootActor(ActorFactory::CreateEmptyActor(Actor::InitDesc{}));
	auto* transform = actor->GetComponentByClass<Transform>();
	Check(transform != nullptr, "Transform pilot actor has a Transform");
	if (!transform) return;
	ComponentPropertyIdentity identity{actor->GetGuid(), typeid(Transform), 0,
		*PropertyPath::FromString("/position")};
	ComponentPropertyEditCommand command(&scene, identity, Vector3::Zero(), Vector3{7.0f, 8.0f, 9.0f});
	Check(command.Execute() && transform->GetLocalPosition().x == 7.0f,
		"Transform inferred property command executes");
	Check(command.Undo() && transform->GetLocalPosition().x == 0.0f,
		"Transform inferred property command undoes");
	Check(command.Execute() && transform->GetLocalPosition().x == 7.0f,
		"Transform inferred property command redoes");
}

void TestRegistryMetadataLifetime()
{
	struct RegistrationProbe : TestComponent { int setting = 0; };
	auto& registry = ComponentRegistry::Get();
	TypeMetadataBuilder<RegistrationProbe> wrongName("DifferentName");
	wrongName.Property("setting", &RegistrationProbe::setting);
	Check(!registry.RegisterReflected<RegistrationProbe>("RegistrationProbe",
		std::make_unique<TypeMetadata>(*wrongName.Build())) && !registry.Has("RegistrationProbe"),
		"Reflected registration rejects mismatched metadata before publishing a factory");
	for (int cycle = 0; cycle < 2; ++cycle)
	{
		TypeMetadataBuilder<RegistrationProbe> builder("ReloadProbe");
		builder.Property("setting", &RegistrationProbe::setting);
		registry.RegisterGameComponent("ReloadProbe", [] { return static_cast<Component*>(new RegistrationProbe); },
			typeid(RegistrationProbe), std::make_unique<TypeMetadata>(*builder.Build()));
		std::unique_ptr<Component> instance(registry.Create("ReloadProbe"));
		Check(instance && registry.GetMetadata("ReloadProbe") &&
			registry.GetMetadata("ReloadProbe")->FindProperty("setting"),
			"GameCode registration publishes factory and metadata on each reload cycle");
		instance.reset();
		registry.UnregisterAllGameComponents();
		Check(!registry.Has("ReloadProbe") && !registry.GetMetadata("ReloadProbe") &&
			registry.GetNameByTypeIndex(typeid(RegistrationProbe)).empty(),
			"GameCode unregister removes metadata and type identity before re-registration");
	}
}

int main()
{
	TestRegistryMetadataLifetime();
	TestTransformPilot();
	TestExecuteUndoRedo();
	return failures == 0 ? 0 : 1;
}
