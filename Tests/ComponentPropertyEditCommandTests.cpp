#include "Command/ComponentPropertyEditCommand.h"
#include "Command/EditorCommandHistory.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Path/PathManager.h"
#include "Tools/BehaviorTemplateGenerator.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

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

	void TestGeneratedBehavior()
	{
		const auto previousRoot = PathManager::IsInitialized() ? std::filesystem::path(PathManager::GetProjectRoot()) : std::filesystem::current_path();
		const auto root = std::filesystem::temp_directory_path() /
			("101EngineBehaviorTemplate-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(root);
		std::ofstream(root / "project.101").close();
		Check(PathManager::Initialize((root / "test.exe").string()), "Template test uses an isolated project");
		Check(BehaviorTemplateGenerator::Generate("GeneratedBehavior"), "Behavior template generates source files");
		auto read = [](const std::filesystem::path& path)
		{
			std::ifstream file(path);
			return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		};
		const auto header = read(root / "Game/GameCode/GeneratedBehavior.h");
		const auto source = read(root / "Game/GameCode/GeneratedBehavior.cpp");
		Check(header.find("REGISTER_GAME_COMPONENT") == std::string::npos &&
			header.find("static std::optional<TypeMetadata> BuildMetadata();") != std::string::npos,
			"Generated header declares metadata without registering the component");
		Check(source.find("REGISTER_GAME_COMPONENT(GeneratedBehavior)") < source.find("GeneratedBehavior::BuildMetadata()") &&
			source.find("std::optional<TypeMetadata> GeneratedBehavior::BuildMetadata()\n{") != std::string::npos &&
			source.find("TypeMetadataBuilder<GeneratedBehavior> builder(\"GeneratedBehavior\");") != std::string::npos &&
			source.find("// builder.Property(\"speed\", &GeneratedBehavior::m_speed);") != std::string::npos,
			"Generated source includes registration, a member builder, and a commented private-member example");
		Check(PathManager::Initialize((previousRoot / "test.exe").string()), "Template test restores the project path");
		std::filesystem::remove_all(root);
	}

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
	struct RegistrationProbe : TestComponent
	{
		int setting = 0;
		static std::optional<TypeMetadata> BuildMetadata()
		{
			TypeMetadataBuilder<RegistrationProbe> builder("ReloadProbe");
			builder.Property("setting", &RegistrationProbe::setting);
			return builder.Build();
		}
	};
	auto& registry = ComponentRegistry::Get();
	TypeMetadataBuilder<RegistrationProbe> wrongName("DifferentName");
	wrongName.Property("setting", &RegistrationProbe::setting);
	Check(!registry.RegisterReflected<RegistrationProbe>("RegistrationProbe",
		std::make_unique<TypeMetadata>(*wrongName.Build())) && !registry.Has("RegistrationProbe"),
		"Reflected registration rejects mismatched metadata before publishing a factory");
	Check(!registry.RegisterGameComponent<RegistrationProbe>("WrongGameName") && !registry.Has("WrongGameName"),
		"GameCode shorthand rejects mismatched metadata before publishing a factory");
	Check(registry.RegisterGameComponent<TestComponent>("EmptyProbe") && registry.GetMetadata("EmptyProbe"),
		"GameCode shorthand supports components without BuildMetadata");
	registry.UnregisterAllGameComponents();
	Check(!registry.Has("EmptyProbe"), "GameCode shorthand without authored metadata is unregistered");
	for (int cycle = 0; cycle < 2; ++cycle)
	{
		Check(registry.RegisterGameComponent<RegistrationProbe>("ReloadProbe"),
			"GameCode shorthand registers authored metadata");
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
	TestGeneratedBehavior();
	TestRegistryMetadataLifetime();
	TestTransformPilot();
	TestExecuteUndoRedo();
	return failures == 0 ? 0 : 1;
}
