#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/ComponentReflection.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/UI/UIimage.h"
#include "Engine/Scene/ComponentRegistry.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>

namespace
{
	int failures = 0;
	void Check(bool condition, const char* name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { ++failures; std::cerr << "[FAIL] " << name << '\n'; }
	}
	void Pilot()
	{
		namespace fs = std::filesystem;
		const auto path = fs::temp_directory_path() / ("101ImprintMaterialization-" + GuidGenerator::Generate().ToString());
		fs::create_directories(path);
		fs::copy_file("Tests/Fixtures/ActorImprint/Minimal.imprint", path / "pilot.imprint");
		const Guid assetGuid = GuidGenerator::Generate();
		Check(MetaFile::Save((path / "pilot.imprint").string(), assetGuid), "Pilot metadata");
		AssetManager assets;
		Check(assets.Initialize(path.string(), nullptr, nullptr), "Pilot catalog");
		ActorImprintSystem system(assets);
		EngineContext context{};
		context.pAssetManager = &assets;
		context.pActorImprintSystem = &system;
		SceneBase scene;
		scene.Initialize(context);
		const auto imprint = system.Load(assetGuid);

		Actor* root = system.Instantiate(scene, imprint, {});
		if (!root) std::cerr << "Operation failed\n";
		Check(root && root->GetOwner() == &scene && scene.GetActorPool().Count() == 1, "Pilot publishes one complete Actor");
		if (root)
		{
			const auto handle = root->GetHandle();
			const auto& registry = scene.GetImprintInstances();
			Check(registry.FindMember(handle) &&
				registry.FindMember(handle)->objectId == 10 &&
				registry.ResolveActor(handle, 10) == root &&
				registry.ResolveComponent(handle, 11),
				"Pilot Registry resolves provenance and objects");
			Check(registry.FindInstance(handle)->assetGuid == assetGuid, "Pilot Registry retains asset provenance");
			Check(!system.Unload(imprint) && !system.Clear(), "Live Instance pins its definition");
			ActorImprintRestoreInput invalid;
			invalid.actorGuids.emplace(10, root->GetGuid());
			Check(!system.RestoreInstance(scene, imprint, invalid) &&
				scene.GetActorPool().Count() == 1 &&
				registry.GetInstances().size() == 1 &&
				scene.ResolveActor(handle) == root,
				"Failed restore leaves Scene and Registry unchanged");
			scene.Finalize();
			Check(registry.GetInstances().empty() &&
				!registry.FindMember(handle) &&
				!scene.ResolveActor(handle),
				"Teardown clears Registry after Actor collection");
			Check(system.Unload(imprint), "Teardown releases definition pin");
		}
		if (fs::absolute(path).parent_path() == fs::absolute(fs::temp_directory_path())) fs::remove_all(path);
	}

	using json = nlohmann::json;
	json Sample()
	{
		std::ifstream stream("Tests/Fixtures/ActorImprint/Minimal.imprint");
		return json::parse(stream);
	}
	template<class T> json Properties()
	{
		T component;
		json properties;
		Check(SerializeReflectedComponent(component, properties), "Built-in properties use existing serializer");
		return properties;
	}
	json LocalRef(LocalObjectId id)
	{
		return { { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", id } };
	}
	struct Fixture
	{
		std::filesystem::path path = std::filesystem::temp_directory_path() /
			("101ImprintMaterialization-" + GuidGenerator::Generate().ToString());
		Guid assetGuid = GuidGenerator::Generate();
		AssetManager assets;
		ActorImprintSystem system{ assets };
		EngineContext context{};
		SceneBase scene;
		ActorImprintHandle imprint;
		explicit Fixture(const json& source, Guid textureGuid = {})
		{
			std::filesystem::create_directories(path);
			if (textureGuid.IsValid())
			{
				std::ofstream(path / "texture.png").put('x');
				MetaFile::Save((path / "texture.png").string(), textureGuid);
			}
			std::ofstream(path / "test.imprint") << source;
			MetaFile::Save((path / "test.imprint").string(), assetGuid);
			Check(assets.Initialize(path.string(), nullptr, nullptr), "Test catalog initializes");
			context.pAssetManager = &assets;
			context.pActorImprintSystem = &system;
			scene.Initialize(context);

			imprint = system.Load(assetGuid);
			if (imprint.IsNull()) std::cerr << "Operation failed\n";
			Check(!imprint.IsNull(), "Test definition loads");
		}
		~Fixture()
		{
			scene.Finalize();
			if (std::filesystem::absolute(path).parent_path() == std::filesystem::absolute(std::filesystem::temp_directory_path()))
				std::filesystem::remove_all(path);
		}
	};

	class MaterializationProbe final : public Component
	{
	public:
		static inline int live = 0, attached = 0, destroyed = 0;
		static inline bool failFactory = false, failProperty = false, failResolve = false, throwResolve = false;
		static inline bool testReentry = false;
		static inline SceneBase* destination = nullptr;
		float weight = 1;
		ActorReference target;
		ActorHandle resolvedHandle;
		MaterializationProbe() { ++live; }
		~MaterializationProbe() override { --live; }
		bool ResolveReferences(SceneBase& scene) override
		{
			Check(&scene != destination &&
				destination->GetImprintInstances().GetInstances().size() < 3,
				"Reference validation runs on an unpublished Scene");
			if (throwResolve) throw std::runtime_error("Injected reference exception");
			if (failResolve) return false;
			Actor* actor = target.Resolve(scene);
			if (!actor) return false;
			resolvedHandle = actor->GetHandle();
			return true;
		}
	private:
		void OnAttachOverride() override
		{
			++attached;
			Actor* actor = GetOwner();
			SceneBase* scene = actor->GetOwner();
			const auto* member = scene->GetImprintInstances().FindMember(actor->GetHandle());
			Check(scene == destination &&
				member &&
				scene->GetImprintInstances().ResolveComponent(member->root,
				scene->GetImprintInstances().FindComponentId(member->root, this)) == this,
				"Attach observes final Scene and complete Registry");
			Check(target.Resolve(*scene) == scene->ResolveActor(resolvedHandle), "Handle resolved before commit survives ownership transfer");
			if (testReentry)
			{
				const auto before = scene->GetAllActors();
				Check(!scene->AddRootActor(ActorFactory::CreateEmptyActor({})), "Attach cannot reenter Actor registration");
				Check(!scene->ReparentActor(actor, nullptr), "Attach cannot reenter hierarchy mutation");
				actor->Destroy();
				scene->EditorUpdate(0);
				Check(!actor->IsDestroyed() && scene->GetAllActors() == before, "Attach cannot trigger Actor destruction or collection");
			}
		}
		void OnStartOverride() override {}
		void PreUpdateOverride(float) override {}
		void UpdateOverride(float) override {}
		void LateUpdateOverride(float) override {}
		void OnDestroyOverride() override { ++destroyed; }
	};
	void RegisterProbe()
	{
		TypeMetadataBuilder<MaterializationProbe> builder("MaterializationProbe");
		builder.Property("weight", &MaterializationProbe::weight).Validate([](float) { return !MaterializationProbe::failProperty; });
		builder.Property("target", &MaterializationProbe::target);
		ComponentRegistry::Get().RegisterGameComponent("MaterializationProbe",
			[]() -> Component* { return MaterializationProbe::failFactory ? nullptr : new MaterializationProbe(); },
			typeid(MaterializationProbe), std::make_unique<TypeMetadata>(*builder.Build()));
	}
	json Hierarchy()
	{
		auto source = Sample();
		auto child = source["actors"][0];
		child["localObjectId"] = 20;
		child["parentLocalObjectId"] = 10;
		child["components"][0]["localObjectId"] = 21;
		child["properties"]["name"] = "Child";
		source["actors"].push_back(child);
		for (int id : { 13, 12 }) source["actors"][0]["components"].push_back({
			{ "localObjectId", id }, { "type", "MaterializationProbe" },
			{ "properties", { { "weight", static_cast<double>(id) }, { "target", LocalRef(20) } } } });
		auto camera = Properties<Camera>();
		camera["targetActorId"] = LocalRef(20);
		source["actors"][0]["components"].push_back({ { "localObjectId", 14 }, { "type", "Camera" }, { "properties", camera } });
		source["nextLocalObjectId"] = 30;
		std::reverse(source["actors"].begin(), source["actors"].end());
		return source;
	}
	void IdentityAndReferences()
	{
		Fixture fixture(Hierarchy());
		MaterializationProbe::destination = &fixture.scene;
		MaterializationProbe::testReentry = true;
		Actor* parent = fixture.scene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		Actor* first = fixture.system.Instantiate(fixture.scene, fixture.imprint, parent->GetHandle());
		AssetReference<ActorImprint> reference; reference.SetGuid(fixture.assetGuid);
		Actor* second = fixture.system.Instantiate(fixture.scene, reference);
		Check(first &&
			second &&
			first->GetParent() == parent &&
			second->GetParent() == nullptr,
			"Two Instances accept ordinary parent and Scene root");
		if (!first || !second) return;
		const auto& registry = fixture.scene.GetImprintInstances();
		Actor* child = registry.ResolveActor(first->GetHandle(), 20);
		Actor* otherChild = registry.ResolveActor(second->GetHandle(), 20);
		Check(child &&
			otherChild &&
			child->GetParent() == first &&
			otherChild->GetParent() == second &&
			child->GetGuid() != otherChild->GetGuid() &&
			first->GetGuid() != second->GetGuid(),
			"Each Instance owns its complete hierarchy and new GUIDs");
		auto* probe = static_cast<MaterializationProbe*>(registry.ResolveComponent(first->GetHandle(), 12));
		auto* other = static_cast<MaterializationProbe*>(registry.ResolveComponent(second->GetHandle(), 12));
		Check(probe &&
			other &&
			probe != other &&
			probe->target.Resolve(fixture.scene) == child &&
			other->target.Resolve(fixture.scene) == otherChild,
			"Local references stay within each independently owned Instance");
		if (probe && other) { probe->weight = 99; Check(other->weight == 12, "Component state is not shared"); }
		auto* camera = static_cast<Camera*>(registry.ResolveComponent(first->GetHandle(), 14));
		Check(camera && camera->GetTargetActorReference().Resolve(fixture.scene) == child, "Built-in Camera uses its existing reference resolver");
		Check(registry.FindMember(child->GetHandle())->root == first->GetHandle() &&
			registry.FindComponentId(first->GetHandle(), probe) == 12,
			"Member and Component identity can be found in both directions");
		ActorImprintRestoreInput restore;
		restore.actorGuids = { { 10, GuidGenerator::Generate() }, { 20, GuidGenerator::Generate() } };
		restore.sourceDefinitionRevision = fixture.system.Resolve(fixture.imprint)->GetRevision();
		restore.rootActorGuid = restore.actorGuids.at(10);
		Actor* restored = fixture.system.RestoreInstance(fixture.scene, fixture.imprint, restore);
		Check(restored &&
			restored->GetGuid() == restore.actorGuids.at(10) &&
			registry.ResolveActor(restored->GetHandle(), 20)->GetGuid() == restore.actorGuids.at(20),
			"Restore uses supplied GUIDs exactly");
		const auto before = fixture.scene.GetAllActors();
		Check(!fixture.system.Instantiate(fixture.scene, fixture.imprint, first->GetHandle()) &&
			!fixture.system.Instantiate(fixture.scene, fixture.imprint, child->GetHandle()) &&
			fixture.scene.GetAllActors() == before,
			"Instance root and member are rejected as external parents");
		MaterializationProbe::testReentry = false;
	}
	void Failures()
	{
		Fixture fixture(Hierarchy());
		MaterializationProbe::destination = &fixture.scene;
		Actor* parent = fixture.scene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		Actor* hole = fixture.scene.AddRootActor(ActorFactory::CreateEmptyActor({}));
		const auto holeHandle = hole->GetHandle();
		hole->Destroy(); fixture.scene.EditorUpdate(0);
		const auto before = fixture.scene.GetAllActors();
		const auto initialAttached = MaterializationProbe::attached;
		const auto initialDestroyed = MaterializationProbe::destroyed;

		for (bool* failure : { &MaterializationProbe::failFactory, &MaterializationProbe::failProperty,
			&MaterializationProbe::failResolve, &MaterializationProbe::throwResolve })
		{
			*failure = true;
			Check(!fixture.system.Instantiate(fixture.scene, fixture.imprint, parent->GetHandle()),
				"Injected construction/property/reference/exception failure rejects the whole Instance");
			*failure = false;
			Check(fixture.scene.GetAllActors() == before &&
				parent->GetDirectChildren().empty() &&
				fixture.scene.ResolveActor(parent->GetGuid()) == parent &&
				fixture.scene.GetImprintInstances().GetInstances().empty(),
				"Failure preserves ActorPool, GUID map, hierarchy and Registry");
			Check(MaterializationProbe::live == 0 &&
				MaterializationProbe::attached == initialAttached &&
				MaterializationProbe::destroyed == initialDestroyed,
				"Unpublished failure destroys objects without lifecycle callbacks");
		}
		Actor* root = fixture.system.Instantiate(fixture.scene, fixture.imprint);
		Check(root &&
			root->GetHandle().index == holeHandle.index &&
			root->GetHandle().generation == holeHandle.generation + 1,
			"Failed transactions consume neither a free Actor slot nor its generation");
		Check(!fixture.system.Instantiate(fixture.scene, ActorImprintHandle{}, {}), "Stale/null Imprint handle is rejected");
		Check(!fixture.system.Instantiate(fixture.scene, fixture.imprint, { 123456, 3 }), "Invalid external parent is rejected");
	}
	void UIAndDepth()
	{
		auto source = Sample();
		source["actors"][0]["components"].push_back({ { "localObjectId", 12 }, { "type", "Canvas" }, { "properties", Properties<Canvas>() } });
		source["nextLocalObjectId"] = 13;
		{
			Fixture fixture(source);
			Check(!fixture.system.Instantiate(fixture.scene, fixture.imprint) &&
				fixture.scene.GetAllActors().empty(),
				"Screen-space Canvas cannot silently replace a definition-owned Transform");
		}
		source["actors"][0]["components"][0]["type"] = "RectTransform";
		source["actors"][0]["components"][0]["properties"] = Properties<RectTransform>();
		{
			Fixture fixture(source);
			Actor* root = fixture.system.Instantiate(fixture.scene, fixture.imprint);
			Check(root && root->GetComponentByClass<RectTransform>(), "Compatible Canvas hierarchy materializes without structural conversion");
		}
		{
			Fixture fixture(Sample());
			auto parentOwned = ActorFactory::CreateEmptyActor({});
			parentOwned->AddComponent(std::make_unique<Canvas>());
			Actor* parent = fixture.scene.AddRootActor(std::move(parentOwned));
			Check(!fixture.system.Instantiate(fixture.scene, fixture.imprint, parent->GetHandle()) &&
				parent->GetDirectChildren().empty(),
				"External Canvas incompatibility leaves the existing parent unchanged");
		}
		auto badReference = Sample();
		auto image = Properties<UIImage>(); image["canvasActorId"] = LocalRef(10);
		badReference["actors"][0]["components"].push_back({ { "localObjectId", 12 }, { "type", "UIImage" }, { "properties", image } });
		badReference["nextLocalObjectId"] = 13;
		{
			Fixture fixture(badReference);
			Check(!fixture.system.Instantiate(fixture.scene, fixture.imprint) &&
				fixture.scene.GetAllActors().empty(),
				"Existing UIImage resolver rejects an Actor without the required Canvas");
		}
		constexpr int count = 2048;
		source = Sample(); const auto actor = source["actors"][0]; source["actors"] = json::array();
		for (int i = count - 1; i >= 0; --i)
		{
			auto entry = actor;
			entry["localObjectId"] = 10 + i * 2;
			entry["parentLocalObjectId"] = i == 0 ? json(nullptr) : json(8 + i * 2);
			entry["components"][0]["localObjectId"] = 11 + i * 2;
			source["actors"].push_back(entry);
		}
		source["nextLocalObjectId"] = 10 + count * 2;
		Fixture deep(source);
		Actor* root = deep.system.Instantiate(deep.scene, deep.imprint);
		Check(root &&
			deep.scene.GetActorPool().Count() == count &&
			deep.scene.GetImprintInstances().ResolveActor(root->GetHandle(), 10 + (count - 1) * 2),
			"Deep reversed hierarchy materializes through the production entry point");
		deep.scene.Finalize();
		Check(deep.scene.GetImprintInstances().GetInstances().empty(), "Deep hierarchy teardown releases every member");
	}
	void AssetAndSceneLifetime()
	{
		const Guid textureGuid = GuidGenerator::Generate();
		auto source = Sample();
		auto sprite = Properties<SpriteRenderer>(); sprite["textureAssetId"] = textureGuid.ToString();
		source["actors"][0]["components"].push_back({ { "localObjectId", 12 }, { "type", "SpriteRenderer" }, { "properties", sprite } });
		source["nextLocalObjectId"] = 13;
		Fixture fixture(source, textureGuid);
		Check(!fixture.system.Instantiate(fixture.scene, fixture.imprint) &&
			fixture.scene.GetActorPool().Count() == 0,
			"Unresolvable runtime AssetReference rejects the entire candidate before attach");
		std::filesystem::remove(fixture.path / "texture.png");
		Check(fixture.assets.Refresh(), "Catalog observes removed referenced asset");
		Check(!fixture.system.Instantiate(fixture.scene, fixture.imprint) &&
			fixture.scene.GetImprintInstances().GetInstances().empty(),
			"Cached definition revalidates AssetReferences against the current catalog");
		Fixture simple(Sample());
		{
			SceneBase another;
			another.Initialize(simple.context);
			Check(simple.system.Instantiate(another, simple.imprint) != nullptr, "Another Scene shares the App-owned definition");
			Check(!simple.system.Clear(), "A reference from another Scene prevents Clear");
		}
		Check(simple.system.Unload(simple.imprint), "Scene destructor releases Instance pins even without explicit Finalize");
	}
	void StructuralPilot()
	{
		Fixture fixture(Hierarchy());
		MaterializationProbe::destination = &fixture.scene;
		Actor* root = fixture.system.Instantiate(fixture.scene, fixture.imprint);
		Check(root != nullptr, "ET-11 pilot creates a real Instance");
		if (!root) return;
		const auto rootHandle = root->GetHandle();
		const auto& registry = fixture.scene.GetImprintInstances();
		Actor* child = registry.ResolveActor(rootHandle, 20);
		const auto childHandle = child->GetHandle();
		bool result = false;
		result = child->Destroy();
		Check(!result &&
			!child->IsDestroyed() &&
			fixture.scene.CanDestroy(child) == false,
			"ET-11 pilot rejects individual member deletion consistently with the query");
		Check(!root->AddComponent(std::make_unique<Canvas>()), "ET-11 pilot routes Component addition through Scene policy");
		Component* probe = registry.ResolveComponent(rootHandle, 12);
		result = probe->MarkForDestruction();
		Check(!result && !probe->IsDestroyed(), "ET-11 pilot routes direct Component destruction through Scene policy");
		Check(!fixture.scene.ReparentActor(child, nullptr) &&
			fixture.scene.CanReparent(child, nullptr) == false,
			"ET-11 pilot rejects member reparent without changing hierarchy");
		result = root->Destroy();
		Check(static_cast<bool>(result) &&
			root->IsDestroyed() &&
			child->IsDestroyed() &&
			registry.FindInstance(rootHandle)->destroying,
			"ET-11 pilot root destruction marks the complete Instance");
		Check(registry.FindMember(rootHandle) &&
			registry.FindMember(childHandle) &&
			!registry.ResolveComponent(rootHandle, 12) &&
			fixture.scene.ResolveActor(root->GetGuid()) == root,
			"ET-11 pilot keeps Actor membership and GUIDs until GC, invalidates Components immediately");
		fixture.scene.EditorUpdate(0);
		Check(!registry.FindMember(childHandle) &&
			registry.GetInstances().empty() &&
			fixture.scene.GetActorPool().Count() == 0 &&
			fixture.system.Unload(fixture.imprint),
			"ET-11 pilot GC removes membership and releases definition pins");
	}
}

int main()
{
	Pilot();
	RegisterProbe();
	IdentityAndReferences();
	Failures();
	UIAndDepth();
	AssetAndSceneLifetime();
	StructuralPilot();
	ComponentRegistry::Get().UnregisterAllGameComponents();
	return failures ? 1 : 0;
}
