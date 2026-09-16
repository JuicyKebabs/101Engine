#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Scene/ActorSubtreeSnapshot.h"
#include "Engine/Scene/ComponentSnapshot.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/UI/Canvas.h"
#include "Command/CreateActorCommand.h"
#include "Command/DeleteActorCommand.h"
#include "Command/DeleteActorImprintInstanceCommand.h"
#include "Command/AddComponentCommand.h"
#include "Command/RemoveComponentCommand.h"
#include "Command/ReparentActorCommand.h"
#include "Command/EditorCommandHistory.h"
#include <fstream>
#include <filesystem>
#include <iostream>

namespace
{
	using Reason = StructuralMutationReason;
	int failures = 0;
	void Check(bool condition, const char* message)
	{
		if (condition) std::cout << "[PASS] " << message << '\n';
		else { ++failures; std::cerr << "[FAIL] " << message << '\n'; }
	}
	template<class T> concept PublicOwnerSetter = requires(T& actor, SceneBase* scene) { actor.SetOwner(scene); };
	template<class T> concept PublicDestroyCallback = requires(T& actor) { actor.OnDestroy(); };
	static_assert(!PublicOwnerSetter<Actor> && !PublicDestroyCallback<Actor> && !PublicDestroyCallback<Component>);
	struct Fixture
	{
		std::filesystem::path path = std::filesystem::temp_directory_path() / ("101ImprintPolicy-" + GuidGenerator::Generate().ToString());
		AssetManager assets;
		ActorImprintSystem system{ assets };
		EngineContext context{};
		SceneBase scene;
		ActorImprintHandle imprint;
		Fixture()
		{
			std::ifstream stream("Tests/Fixtures/ActorImprint/Minimal.imprint");
			auto source = nlohmann::json::parse(stream);
			Camera camera;
			nlohmann::json cameraProperties;
			camera.Serialize(cameraProperties);
			source["actors"][0]["components"].push_back({ { "localObjectId", 12 }, { "type", "Camera" }, { "properties", cameraProperties } });
			auto child = source["actors"][0];
			child["components"].erase(child["components"].begin() + 1);
			child["localObjectId"] = 20; child["parentLocalObjectId"] = 10;
			child["components"][0]["localObjectId"] = 21;
			source["actors"].push_back(child); source["nextLocalObjectId"] = 22;
			std::filesystem::create_directories(path);
			std::ofstream(path / "test.imprint") << source;
			const auto guid = GuidGenerator::Generate();
			MetaFile::Save((path / "test.imprint").string(), guid);
			Check(assets.Initialize(path.string(), nullptr, nullptr), "Policy fixture catalog initializes");
			imprint = system.Load(guid);
			context.pAssetManager = &assets; context.pActorImprintSystem = &system;
			scene.Initialize(context);
		}
		~Fixture()
		{
			scene.Finalize();
			if (std::filesystem::absolute(path).parent_path() == std::filesystem::absolute(std::filesystem::temp_directory_path()))
				std::filesystem::remove_all(path);
		}
		Actor* Ordinary(Actor* parent = nullptr)
		{
			auto actor = ActorFactory::CreateEmptyActor({});
			return parent ? scene.AddChildActor(std::move(actor), parent->GetHandle()) : scene.AddRootActor(std::move(actor));
		}
	};

	void MutationMatrix()
	{
		Fixture f;
		Actor* parent = f.Ordinary();
		Actor* other = f.Ordinary(parent);
		Actor* root = f.system.Instantiate(f.scene, f.imprint, parent->GetHandle());
		Check(root != nullptr, "Real Instance materializes for mutation matrix"); if (!root) return;
		const auto& registry = f.scene.GetImprintInstances();
		Actor* child = registry.ResolveActor(root->GetHandle(), 20);
		StructuralMutationResult result;
		Check(f.scene.CanReparent(parent, other).reason == Reason::HierarchyCycle, "Cycle has a distinct reason");
		SceneBase foreignScene;
		Actor* foreign = foreignScene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		Check(f.scene.CanDestroy(foreign).reason == Reason::ForeignScene, "Foreign Scene has a distinct reason");
		Check(f.scene.CanDestroy(ActorHandle{ 123456, 7 }).reason == Reason::StaleHandle, "Stale handle has a distinct reason");
		Check(f.scene.CanDestroy(static_cast<Actor*>(nullptr)).reason == Reason::InvalidActor, "Null Actor has a distinct reason");
		Check(f.scene.CanAddComponent(parent, typeid(Transform)).reason == Reason::ComponentPolicyViolation,
			"Ordinary Actor retains its Component cardinality policy");
		Check(!root->AddComponent<Canvas>() && !f.scene.AddActorComponentImmediate(root, std::make_unique<Camera>(), 0, &result) &&
			result.reason == Reason::ImprintMemberImmutable, "Both runtime and immediate Component additions reject Instance members");
		Component* transform = registry.ResolveComponent(root->GetHandle(), 11);
		root->RemoveComponentByClass<Camera>(&result);
		Check(result.reason == Reason::ImprintMemberImmutable && !root->GetComponentByClass<Camera>()->IsDestroyed(),
			"Templated removal cannot bypass Instance policy");
		transform->MarkForDestruction(&result);
		Check(result.reason == Reason::ImprintMemberImmutable && !transform->IsDestroyed() &&
			!f.scene.RemoveActorComponentImmediate(root, transform, &result), "Direct and immediate Component removal preserve Instance structure");
		result = {};
		Check(!root->AddChild(ActorFactory::CreateEmptyActor({}), &result) && result == f.scene.CanAddChildActor(root),
			"Actor child insertion reports the same Instance refusal as its query");
		result = {};
		Check(!f.scene.AddChildActor(ActorFactory::CreateEmptyActor({}), child->GetHandle(), &result) &&
			result == f.scene.CanAddChildActor(child), "Scene child insertion reports the same Instance refusal as its query");
		Check(!f.scene.ReparentActor(other, root, &result) && result == f.scene.CanReparent(other, root),
			"Ordinary Actor cannot enter an Instance hierarchy");
		Check(!f.scene.ReparentActor(child, other, &result) && result == f.scene.CanReparent(child, other),
			"Non-root member cannot leave its hierarchy");
		Check(f.scene.ReparentActor(root, other, &result) && root->GetParent() == other,
			"Instance root can move beneath an ordinary Actor");
		Check(f.scene.ReparentActor(root, nullptr, &result) && !root->GetParent(), "Instance root can move to Scene root");
		Check(f.scene.ReparentActor(root, parent), "Instance returns to its ordinary parent");
		Check(f.scene.CanAddComponent(parent, typeid(Canvas)).reason == Reason::UIConstraintViolation && !parent->AddComponent<Canvas>() &&
			!parent->GetComponentByClass<Canvas>(), "Adding Canvas to an ordinary ancestor cannot indirectly convert Instance transforms");
		auto canvasOwned = ActorFactory::CreateEmptyActor({}); canvasOwned->AddComponent<Canvas>();
		Actor* canvasActor = f.scene.AddRootActor(std::move(canvasOwned));
		Check(!f.scene.ReparentActor(parent, canvasActor, &result) && result.reason == Reason::UIConstraintViolation && !parent->GetParent(),
			"Moving an ordinary subtree cannot indirectly convert Instance transforms");
		ActorSubtreeSnapshot snapshot;
		Check(!snapshot.Capture(parent, &f.scene), "Ordinary subtree snapshots refuse embedded Instance provenance");
		const auto rootHandle = root->GetHandle(), childHandle = child->GetHandle();
		ActorImprintRestoreInput saved;
		const auto* savedRecord = registry.FindInstance(rootHandle);
		saved.sourceDefinitionRevision = savedRecord->sourceRevision;
		saved.rootActorGuid = savedRecord->actors.at(savedRecord->rootId).guid;
		for (const auto& [id, actor] : savedRecord->actors) saved.actorGuids.emplace(id, actor.guid);
		Check(f.scene.RemoveActor(parent, true, &result) && parent->IsDestroyed() && root->IsDestroyed() && child->IsDestroyed(),
			"Ordinary cascade treats each included Instance as a complete destruction unit");
		Check(f.scene.CanDestroy(root).reason == Reason::PendingDestroy && registry.FindMember(childHandle) &&
			!f.system.RestoreInstance(f.scene, f.imprint, saved), "Destroying membership stays indexed and GUIDs remain reserved until GC");
		f.scene.EditorUpdate(0);
		Check(!registry.FindMember(rootHandle) && !registry.FindMember(childHandle), "GC retires every reverse index entry");
		Check(f.system.RestoreInstance(f.scene, f.imprint, saved) != nullptr, "Saved identity becomes restorable after actual collection");
	}

	void Commands()
	{
		Fixture f;
		Actor* root = f.system.Instantiate(f.scene, f.imprint);
		if (!root) { Check(false, "Command fixture Instance"); return; }
		Actor* child = f.scene.GetImprintInstances().ResolveActor(root->GetHandle(), 20);
		CreateActorCommand create(&f.scene, {}, root->GetGuid());
		Check(!create.Execute() && create.GetStructuralResult().reason == Reason::ImprintMemberImmutable, "Create command reports insertion refusal");
		AddComponentCommand add(&f.scene, root->GetGuid(), "Camera");
		Check(!add.Execute() && add.GetStructuralResult().reason == Reason::ImprintMemberImmutable, "Add Component command reports policy refusal");
		RemoveComponentCommand remove(&f.scene, root->GetGuid(), "Transform", 0);
		Check(!remove.Execute() && remove.GetStructuralResult().reason == Reason::ImprintMemberImmutable, "Remove Component command reports policy refusal");
		ReparentActorCommand reparent(&f.scene, child->GetGuid(), {});
		Check(!reparent.Execute() && reparent.GetStructuralResult().reason == Reason::ImprintMemberImmutable, "Reparent command reports member refusal");
		DeleteActorCommand destroy(&f.scene, root->GetGuid());
		Check(!destroy.Execute() && destroy.GetStructuralResult().reason == Reason::InstanceSnapshotRequired && !root->IsDestroyed(),
			"Ordinary Delete command requires an Instance snapshot instead of discarding provenance");
		DeleteActorImprintInstanceCommand destroyMember(f.scene, f.system, child->GetGuid());
		Check(!destroyMember.Execute() && destroyMember.GetStructuralResult().reason == Reason::InstanceDestroyRequired &&
			!root->IsDestroyed(), "Instance Delete command requires the root Actor");
		EditorCommandHistory history;
		Check(!history.Execute(std::make_unique<AddComponentCommand>(&f.scene, root->GetGuid(), "Camera")) && !history.CanUndo(),
			"Failed structural commands do not enter history");
		Check(history.GetLastStructuralResult().reason == Reason::ImprintMemberImmutable,
			"Command history preserves the structural reason after destroying a failed command");
		ComponentSnapshot componentSnapshot;
		Check(componentSnapshot.Capture(root, root->GetComponentByClass<Camera>()), "Component snapshot captures an Instance value");
		StructuralMutationResult result;
		Check(!componentSnapshot.Restore(&f.scene, &result) && result.reason == Reason::ImprintMemberImmutable,
			"Component snapshot restore propagates the Scene policy reason");
	}

	class RemovalProbe final : public Component
	{
	public:
		static inline int notifications = 0;
		static inline int attachments = 0;
		static inline int detachments = 0;
		static inline int blockedMutations = 0;
		static inline SceneBase* scene = nullptr;
		static inline Actor* survivor = nullptr;
		int value = 0;
	private:
		static void TryStructuralReentry()
		{
			if (scene && !scene->AddRootActor(ActorFactory::CreateEmptyActor({}))) ++blockedMutations;
			StructuralMutationResult result;
			if (survivor && !survivor->AddComponent(std::make_unique<Camera>(), &result) &&
				result.reason == Reason::TransactionInProgress) ++blockedMutations;
		}
		void OnAttachOverride() override { ++attachments; TryStructuralReentry(); }
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDetachOverride() override { ++detachments; TryStructuralReentry(); }
		void OnDestroyOverride() override { ++notifications; TryStructuralReentry(); }
	};
	void DeferredNotification()
	{
		RemovalProbe::notifications = RemovalProbe::attachments = RemovalProbe::detachments = RemovalProbe::blockedMutations = 0;
		TypeMetadataBuilder<RemovalProbe> builder("RemovalProbe"); builder.Property("value", &RemovalProbe::value);
		ComponentRegistry::Get().RegisterGameComponent("RemovalProbe", []() -> Component* { return new RemovalProbe(); },
			typeid(RemovalProbe), std::make_unique<TypeMetadata>(*builder.Build()));
		Fixture f;
		Actor* survivor = f.Ordinary();
		Actor* actor = f.Ordinary();
		RemovalProbe::scene = &f.scene;
		RemovalProbe::survivor = survivor;
		auto* probe = actor->AddComponent<RemovalProbe>();
		Check(probe && RemovalProbe::attachments == 1 && RemovalProbe::blockedMutations == 2 &&
			!survivor->GetComponentByClass<Camera>(), "Attach callbacks cannot reenter Scene structure");
		probe->MarkForDestruction();
		Check(probe->IsDestroyed() && RemovalProbe::notifications == 0, "Deferred removal postpones the lifecycle notification");
		actor->LateUpdate(0);
		Check(!actor->GetComponentByClass<RemovalProbe>() && RemovalProbe::notifications == 1 &&
			RemovalProbe::detachments == 1 && RemovalProbe::blockedMutations == 6 &&
			!survivor->GetComponentByClass<Camera>(), "Deferred collection calls lifecycle once and blocks callback reentry");
		auto* second = actor->AddComponent<RemovalProbe>();
		const ActorHandle actorHandle = actor->GetHandle();
		Check(second && f.scene.RemoveActor(actor), "Actor destruction probe enters deferred GC");
		f.scene.EditorUpdate(0);
		Check(!f.scene.ResolveActor(actorHandle) && RemovalProbe::notifications == 2 &&
			RemovalProbe::detachments == 2 && RemovalProbe::blockedMutations == 12 &&
			!survivor->GetComponentByClass<Camera>(), "ActorPool GC blocks lifecycle reentry without invalidating its storage");
		RemovalProbe::scene = nullptr;
		RemovalProbe::survivor = nullptr;
		f.scene.Finalize();
		Check(RemovalProbe::notifications == 2, "Later Scene teardown does not repeat a Component notification");
		ComponentRegistry::Get().UnregisterAllGameComponents();
	}

	void UITransitionBoundaries()
	{
		Fixture f;
		Actor* canvasParent = f.Ordinary();
		Check(canvasParent->AddComponent<Canvas>() != nullptr, "Non-cascade fixture creates a Canvas parent");
		Actor* child = f.Ordinary(canvasParent);
		Check(dynamic_cast<RectTransform*>(child->GetComponentByClass<Transform>()) != nullptr,
			"Child starts with its Canvas-required RectTransform");
		StructuralMutationResult result;
		Check(f.scene.RemoveActor(canvasParent, false, &result) && !child->GetParent() &&
			std::type_index(typeid(*child->GetComponentByClass<Transform>())) == typeid(Transform),
			"Non-cascade destruction commits the prevalidated UI-aware detach");

		Actor* pendingCanvasOwner = f.Ordinary();
		Canvas* pendingCanvas = pendingCanvasOwner->AddComponent<Canvas>();
		Check(pendingCanvas && dynamic_cast<RectTransform*>(pendingCanvasOwner->GetComponentByClass<Transform>()),
			"Pending-Canvas fixture starts in Screen-Space UI state");
		pendingCanvas->MarkForDestruction(&result);
		Check(result && pendingCanvas->IsDestroyed() && !pendingCanvasOwner->GetComponentByClass<Canvas>() &&
			std::type_index(typeid(*pendingCanvasOwner->GetComponentByClass<Transform>())) == typeid(Transform),
			"A deferred Canvas becomes absent and updates derived UI state at mark time");
		Actor* instance = f.system.Instantiate(f.scene, f.imprint, pendingCanvasOwner->GetHandle());
		Check(instance != nullptr, "Materialization beneath a pending Canvas uses the post-removal hierarchy");
		pendingCanvasOwner->LateUpdate(0);
		Check(instance && instance->GetParent() == pendingCanvasOwner &&
			std::type_index(typeid(*instance->GetComponentByClass<Transform>())) == typeid(Transform),
			"Physical Canvas collection preserves the already validated Instance hierarchy");
	}
}

int main()
{
	MutationMatrix();
	Commands();
	DeferredNotification();
	UITransitionBoundaries();
	return failures ? 1 : 0;
}
