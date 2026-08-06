#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/Canvas.h"

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

	class MutationTestComponent final : public Component
	{
	private:
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override {}
	};

	void RegisterMutationTestComponent()
	{
		ComponentRegistry::Get().Register(
			"MutationTestComponent",
			[]() { return static_cast<Component*>(new MutationTestComponent()); },
			std::type_index(typeid(MutationTestComponent)),
			ComponentCardinality::Multiple,
			ComponentFamily::None
		);
	}

	std::unique_ptr<Component> MakeMutationComponent(const std::string& name)
	{
		auto component = std::make_unique<MutationTestComponent>();
		component->SetName(name);
		return component;
	}

	Actor* AddEmptyActor(SceneBase& scene, const std::string& name)
	{
		return scene.AddRootActor(ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, name)));
	}

	void TestImmediateInsertionAndRemoval()
	{
		SceneBase scene;
		Actor* actor = AddEmptyActor(scene, "MutationActor");

		Component* first = scene.AddActorComponentImmediate(
			actor, MakeMutationComponent("First"), 0);
		Component* third = scene.AddActorComponentImmediate(
			actor, MakeMutationComponent("Third"), 1);
		Component* second = scene.AddActorComponentImmediate(
			actor, MakeMutationComponent("Second"), 1);

		const std::type_index typeId = typeid(MutationTestComponent);
		std::vector<Component*> components = actor->GetComponentsByExactType(typeId);

		Check(first && second && third,
			"Scene adds components immediately");
		Check(components.size() == 3 &&
			components[0] == first &&
			components[1] == second &&
			components[2] == third,
			"Immediate addition inserts at the requested exact-type occurrence index");

		Component* rejected = scene.AddActorComponentImmediate(
			actor, MakeMutationComponent("Rejected"), 4);
		components = actor->GetComponentsByExactType(typeId);

		Check(rejected == nullptr && components.size() == 3,
			"Invalid occurrence index is rejected without adding a component");

		Check(scene.RemoveActorComponentImmediate(actor, second),
			"Scene removes a component immediately");

		components = actor->GetComponentsByExactType(typeId);
		Check(components.size() == 2 &&
			components[0] == first &&
			components[1] == third,
			"Immediate removal preserves the remaining exact-type order");
	}

	void TestImmediateRemovalValidation()
	{
		SceneBase scene;
		Actor* owner = AddEmptyActor(scene, "Owner");
		Actor* other = AddEmptyActor(scene, "Other");

		Component* component = scene.AddActorComponentImmediate(
			owner, MakeMutationComponent("Owned"), 0);
		Transform* transform = owner->GetComponentByClass<Transform>();

		Check(!scene.RemoveActorComponentImmediate(other, component),
			"Scene rejects removal through a different Actor");
		Check(owner->GetComponentsByExactType(
			std::type_index(typeid(MutationTestComponent))).size() == 1,
			"Rejected foreign removal preserves the component");
		Check(!scene.RemoveActorComponentImmediate(owner, transform),
			"Scene rejects removal of a UniqueRequired Transform");
		Check(owner->CountComponentFamily(ComponentFamily::Transform) == 1,
			"Rejected Transform removal preserves the Transform family invariant");
	}

	void TestCanvasRemovalReappliesUIHierarchyConstraints()
	{
		SceneBase scene;
		Actor* actor = AddEmptyActor(scene, "CanvasActor");

		Component* canvas = scene.AddActorComponentImmediate(
			actor, std::make_unique<Canvas>(), 0);

		Check(canvas != nullptr &&
			actor->GetComponentByClass<RectTransform>() != nullptr,
			"Adding a root Screen-Space Canvas converts Transform to RectTransform");

		Check(scene.RemoveActorComponentImmediate(actor, canvas),
			"Scene removes a Canvas immediately");
		Check(actor->GetComponentByClass<RectTransform>() == nullptr &&
			actor->GetComponentByClass<Transform>() != nullptr,
			"Removing the root Canvas reapplies hierarchy constraints and restores Transform");
	}
}

int main()
{
	RegisterMutationTestComponent();
	TestImmediateInsertionAndRemoval();
	TestImmediateRemovalValidation();
	TestCanvasRemovalReappliesUIHierarchyConstraints();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " immediate Component mutation test(s) failed.\n";
		return 1;
	}

	std::cout << "All immediate Component mutation tests passed.\n";
	return 0;
}
