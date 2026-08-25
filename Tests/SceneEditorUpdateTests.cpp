#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Component.h"
#include "Engine/Component/Behavior.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"

#include <cmath>
#include <iostream>
#include <string>
#include <typeindex>

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

	bool NearlyEqual(float lhs, float rhs)
	{
		return std::abs(lhs - rhs) < 0.0001f;
	}

	struct LifecycleCounts
	{
		int attach = 0;
		int start = 0;
		int preUpdate = 0;
		int update = 0;
		int lateUpdate = 0;
		int detach = 0;
		int destroy = 0;
	};

	class LifecycleTestComponent final : public Component
	{
	public:
		explicit LifecycleTestComponent(LifecycleCounts* counts = nullptr)
			: m_counts(counts)
		{
		}

	private:
		void OnAttachOverride() override { if (m_counts) ++m_counts->attach; }
		void OnStartOverride() override { if (m_counts) ++m_counts->start; }
		void PreUpdateOverride(float) override { if (m_counts) ++m_counts->preUpdate; }
		void UpdateOverride(float) override { if (m_counts) ++m_counts->update; }
		void LateUpdateOverride(float) override { if (m_counts) ++m_counts->lateUpdate; }
		void OnDetachOverride() override { if (m_counts) ++m_counts->detach; }
		void OnDestroyOverride() override { if (m_counts) ++m_counts->destroy; }

		LifecycleCounts* m_counts = nullptr;
	};

	class LifecycleTestBehavior final : public Behavior
	{
	public:
		explicit LifecycleTestBehavior(LifecycleCounts* counts = nullptr)
			: m_counts(counts)
		{
		}

		void Start() override { if (m_counts) ++m_counts->start; }
		void PreUpdate() override { if (m_counts) ++m_counts->preUpdate; }
		void Update() override { if (m_counts) ++m_counts->update; }
		void LateUpdate() override { if (m_counts) ++m_counts->lateUpdate; }
		void Destroy() override { if (m_counts) ++m_counts->destroy; }

	private:
		LifecycleCounts* m_counts = nullptr;
	};

	void RegisterLifecycleTestComponent()
	{
		ComponentRegistry::Get().RegisterPolicy(
			std::type_index(typeid(LifecycleTestComponent)),
			{ ComponentCardinality::Multiple, ComponentFamily::None }
		);

		ComponentRegistry::Get().RegisterPolicy(
			std::type_index(typeid(LifecycleTestBehavior)),
			{ ComponentCardinality::Multiple, ComponentFamily::None }
		);
	}

	Actor* AddRoot(SceneBase& scene, const char* name)
	{
		return scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, name)));
	}

	void TestEditorUpdateDoesNotRunGameplayCallbacks()
	{
		SceneBase scene;
		LifecycleCounts counts;
		Actor* actor = AddRoot(scene, "LifecycleActor");
		auto* component = actor->AddComponent<LifecycleTestComponent>(&counts);

		Check(component != nullptr && counts.attach == 1,
			"A component added to a registered Actor is attached immediately");
		Check(component && !component->IsStarted(),
			"Attaching a component does not start its gameplay lifecycle");

		scene.EditorUpdate(1.0f / 60.0f);
		scene.EditorUpdate(1.0f / 60.0f);

		Check(counts.start == 0 &&
			counts.preUpdate == 0 &&
			counts.update == 0 &&
			counts.lateUpdate == 0,
			"EditorUpdate does not execute gameplay lifecycle callbacks");
		Check(component && !component->IsStarted(),
			"EditorUpdate leaves the component in the not-started state");
	}

	void TestRuntimeLifecycleStartsOnceAndUpdatesEveryFrame()
	{
		SceneBase scene;
		LifecycleCounts counts;
		Actor* actor = AddRoot(scene, "RuntimeActor");
		auto* component = actor->AddComponent<LifecycleTestComponent>(&counts);

		for (int frame = 0; frame < 2; ++frame)
		{
			scene.PreUpdate(1.0f / 60.0f);
			scene.Update(1.0f / 60.0f);
			scene.LateUpdate(1.0f / 60.0f);
		}

		Check(component && component->IsStarted() && counts.start == 1,
			"Runtime update starts a component exactly once");
		Check(counts.preUpdate == 2 && counts.update == 2 && counts.lateUpdate == 2,
			"Runtime update executes each gameplay callback once per frame");
	}

	void TestEditorUpdateFlushesTransformHierarchy()
	{
		SceneBase scene;
		Actor* parent = AddRoot(scene, "Parent");
		Actor* child = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "Child")),
			parent->GetHandle());

		Transform* parentTransform = parent->GetComponentByClass<Transform>();
		Transform* childTransform = child->GetComponentByClass<Transform>();
		parentTransform->SetLocalPosition({ 10.0f, 0.0f, 0.0f });
		childTransform->SetLocalPosition({ 2.0f, 0.0f, 0.0f });

		scene.EditorUpdate(0.0f);

		Check(NearlyEqual(childTransform->GetWorldPosition().x, 12.0f),
			"EditorUpdate flushes parent and child transforms in hierarchy order");
	}

	void TestEditorUpdateCollectsDestroyedActors()
	{
		SceneBase scene;
		Actor* actor = AddRoot(scene, "DestroyedActor");
		const Guid actorGuid = actor->GetGuid();

		scene.RemoveActor(actor);
		Check(actor->IsDestroyed(),
			"Actor removal can be requested before EditorUpdate");
		Check(scene.ResolveActor(actorGuid) != nullptr,
			"A pending-destroy Actor remains resolvable until collection");

		scene.EditorUpdate(0.0f);

		Check(scene.ResolveActor(actorGuid) == nullptr,
			"EditorUpdate collects destroyed Actors and clears their Guid mappings");
	}

	void TestFinalizeDestroysEntireHierarchyExactlyOnce()
	{
		SceneBase scene;
		LifecycleCounts componentCounts;
		LifecycleCounts behaviorCounts;

		Actor* parent = AddRoot(scene, "FinalizeParent");
		Actor* child = scene.AddChildActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "FinalizeChild")),
			parent->GetHandle());

		const Guid parentGuid = parent->GetGuid();
		const Guid childGuid = child->GetGuid();

		parent->AddComponent<LifecycleTestComponent>(&componentCounts);
		child->AddComponent<LifecycleTestComponent>(&componentCounts);
		child->AddComponent<LifecycleTestBehavior>(&behaviorCounts);

		// Start the runtime lifecycle so this also represents stopping a Play Scene.
		scene.PreUpdate(0.0f);
		Check(behaviorCounts.start == 1,
			"Runtime Behavior starts before the Scene is finalized");

		scene.Finalize();

		Check(componentCounts.attach == 2 &&
			componentCounts.detach == 2 &&
			componentCounts.destroy == 2,
			"Finalize detaches and destroys every Component in the hierarchy");
		Check(behaviorCounts.destroy == 1,
			"Finalize executes Behavior::Destroy for a started Play Scene");
		Check(scene.GetActorPool().Count() == 0,
			"Finalize releases every parent and child Actor");
		Check(scene.ResolveActor(parentGuid) == nullptr &&
			scene.ResolveActor(childGuid) == nullptr,
			"Finalize clears every Actor Guid mapping");

		scene.Finalize();

		Check(componentCounts.detach == 2 &&
			componentCounts.destroy == 2 &&
			behaviorCounts.destroy == 1,
			"Calling Finalize twice does not repeat lifecycle callbacks");
	}
}

int main()
{
	RegisterLifecycleTestComponent();

	TestEditorUpdateDoesNotRunGameplayCallbacks();
	TestRuntimeLifecycleStartsOnceAndUpdatesEveryFrame();
	TestEditorUpdateFlushesTransformHierarchy();
	TestEditorUpdateCollectsDestroyedActors();
	TestFinalizeDestroysEntireHierarchyExactlyOnce();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " Scene EditorUpdate test(s) failed.\n";
		return 1;
	}

	std::cout << "All Scene EditorUpdate tests passed.\n";
	return 0;
}
