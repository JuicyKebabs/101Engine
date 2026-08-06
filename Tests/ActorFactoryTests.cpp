#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/RectTransform.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/UI/UIImage.h"

#include <algorithm>
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

	void TestNewActorGuidGeneration()
	{
		const Actor::InitDesc desc(true, TAG_NONE, "Generated");
		auto first = ActorFactory::CreateActor(ActorType::Camera, desc);
		auto second = ActorFactory::CreateActor(ActorType::Camera, desc);

		Check(first != nullptr && second != nullptr, "CreateActor creates actors");
		Check(first && first->GetGuid().IsValid(), "CreateActor assigns a valid Guid");
		Check(second && second->GetGuid().IsValid(), "Each created actor has a valid Guid");
		Check(first && second && first->GetGuid() != second->GetGuid(),
			"Separate CreateActor calls generate different Guids");
		Check(first && first->HasComponent<Camera>(),
			"CreateActor preserves ActorType component construction");
	}

	void TestGuidStringRoundTrip()
	{
		const Guid original = GuidGenerator::Generate();
		const std::string serialized = original.ToString();
		Guid restored;

		Check(!serialized.empty(), "Guid serializes to a non-empty string");
		Check(Guid::TryParse(serialized, restored), "Serialized Guid parses successfully");
		Check(restored == original, "Guid string round trip preserves every byte");
	}

	void TestNewEmptyActorGuidGeneration()
	{
		const Actor::InitDesc desc(true, TAG_NONE, "Empty");
		auto actor = ActorFactory::CreateEmptyActor(desc);

		Check(actor != nullptr, "CreateEmptyActor creates an actor");
		Check(actor && actor->GetGuid().IsValid(), "CreateEmptyActor assigns a valid Guid");
		Check(actor && actor->HasComponent<Transform>(),
			"CreateEmptyActor preserves its default Transform");
	}

	void TestActorRestoration()
	{
		const Actor::InitDesc desc(true, TAG_NONE, "Restored");
		const Guid persistedGuid = GuidGenerator::Generate();
		auto actor = ActorFactory::RestoreActor(ActorType::Camera, desc, persistedGuid);

		Check(actor != nullptr, "RestoreActor creates an actor from a valid Guid");
		Check(actor && actor->GetGuid() == persistedGuid,
			"RestoreActor preserves the supplied Guid exactly");
		Check(actor && actor->HasComponent<Camera>(),
			"RestoreActor preserves ActorType component construction");
	}

	void TestEmptyActorRestoration()
	{
		const Actor::InitDesc desc(true, TAG_NONE, "RestoredEmpty");
		const Guid persistedGuid = GuidGenerator::Generate();
		auto actor = ActorFactory::RestoreEmptyActor(desc, persistedGuid);

		Check(actor != nullptr, "RestoreEmptyActor creates an actor from a valid Guid");
		Check(actor && actor->GetGuid() == persistedGuid,
			"RestoreEmptyActor preserves the supplied Guid exactly");
		Check(actor && actor->HasComponent<Transform>(),
			"RestoreEmptyActor preserves its default Transform");
	}

	void TestTransformFamilyExclusion()
	{
		auto actor = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "TransformActor"));

		RectTransform* rectTransform = actor->AddComponent<RectTransform>();

		Check(rectTransform == nullptr,
			"Actor rejects RectTransform when Transform already exists");
		Check(actor->CountComponentFamily(ComponentFamily::Transform) == 1,
			"Rejected RectTransform leaves exactly one Transform-family component");
		Check(actor->GetComponentByClass<Transform>() != nullptr,
			"Rejected RectTransform preserves the existing Transform");
	}

	void TestRectTransformFamilyExclusion()
	{
		auto actor = ActorFactory::CreateActor(
			ActorType::UI,
			Actor::InitDesc(true, TAG_NONE, "UIActor"));

		Transform* transform = actor->AddComponent<Transform>();

		Check(actor->GetComponentByClass<RectTransform>() != nullptr,
			"UI Actor is created with RectTransform");
		Check(transform == nullptr,
			"Actor rejects Transform when RectTransform already exists");
		Check(actor->CountComponentFamily(ComponentFamily::Transform) == 1,
			"Rejected Transform leaves exactly one Transform-family component");
	}

	void TestRegistryUsesTransformFamilyPolicy()
	{
		auto actor = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "RegistryActor"));

		const bool added =
			ComponentRegistry::Get().AddToActor("RectTransform", actor.get());

		Check(!added,
			"ComponentRegistry reports Transform-family conflicts");
		Check(actor->CountComponentFamily(ComponentFamily::Transform) == 1,
			"Registry conflict does not add a second Transform-family component");
	}

	void TestUniqueComponentDuplicateRejection()
	{
		auto actor = ActorFactory::CreateActor(
			ActorType::Camera,
			Actor::InitDesc(true, TAG_NONE, "CameraActor"));

		Camera* duplicate = actor->AddComponent<Camera>();

		Check(duplicate == nullptr,
			"Actor rejects a duplicate UniqueOptional component");
		Check(actor->GetComponentsByClass<Camera>().size() == 1,
			"Duplicate rejection preserves exactly one Camera");
	}

	void TestRendererFamilyExclusion()
	{
		auto actor = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "RendererActor"));

		MeshRenderer* meshRenderer = actor->AddComponent<MeshRenderer>();
		SpriteRenderer* spriteRenderer = actor->AddComponent<SpriteRenderer>();

		Check(meshRenderer != nullptr,
			"Actor accepts the first Renderer-family component");
		Check(spriteRenderer == nullptr,
			"Actor rejects a different Renderer-family component");
		Check(actor->CountComponentFamily(ComponentFamily::Renderer) == 1,
			"Renderer-family conflict leaves exactly one renderer");
		Check(actor->GetComponentByClass<MeshRenderer>() == meshRenderer,
			"Renderer-family conflict preserves the existing renderer");
	}

	void TestRegistryUsesRendererFamilyPolicy()
	{
		auto actor = ActorFactory::CreateEmptyActor(
			Actor::InitDesc(true, TAG_NONE, "RegistryRendererActor"));

		const bool addedMesh =
			ComponentRegistry::Get().AddToActor("MeshRenderer", actor.get());
		const bool canAddImage =
			ComponentRegistry::Get().CanAddToActor("UIImage", actor.get());
		const bool addedImage =
			ComponentRegistry::Get().AddToActor("UIImage", actor.get());

		Check(addedMesh,
			"ComponentRegistry adds the first Renderer-family component");
		Check(!canAddImage,
			"ComponentRegistry reports Renderer-family conflicts before creation");
		Check(!addedImage,
			"ComponentRegistry rejects a conflicting Renderer-family component");
		Check(actor->CountComponentFamily(ComponentFamily::Renderer) == 1,
			"Registry conflict leaves exactly one Renderer-family component");
	}

	void TestRegisteredComponentNamesAreSorted()
	{
		const std::vector<std::string> names =
			ComponentRegistry::Get().GetRegisteredComponentNames();

		Check(!names.empty(),
			"ComponentRegistry returns registered component names");
		Check(std::is_sorted(names.begin(), names.end()),
			"ComponentRegistry returns component names in alphabetical order");
	}

	void TestSceneRequiresOneTransformFamilyComponent()
	{
		SceneBase scene;
		const Guid invalidGuid = GuidGenerator::Generate();
		auto invalid = ActorFactory::RestoreActorShell(
			Actor::InitDesc(true, TAG_NONE, "InvalidActor"),
			invalidGuid);

		Check(scene.AddRootActor(std::move(invalid)) == nullptr,
			"Scene rejects an Actor without a Transform-family component");
		Check(scene.ResolveActor(invalidGuid) == nullptr,
			"Rejected Actor does not enter the Scene");

		const Guid validGuid = GuidGenerator::Generate();
		auto valid = ActorFactory::RestoreActorShell(
			Actor::InitDesc(true, TAG_NONE, "ValidUIActor"),
			validGuid);
		Check(valid->AddComponent<RectTransform>() != nullptr,
			"Restoration shell accepts a RectTransform");

		Actor* registered = scene.AddRootActor(std::move(valid));

		Check(registered != nullptr,
			"Scene accepts an Actor with exactly one Transform-family component");
		Check(registered &&
			registered->GetComponentByClass<RectTransform>() == nullptr &&
			registered->GetComponentByClass<Transform>() != nullptr &&
			registered->CountComponentFamily(
				ComponentFamily::Transform) == 1,
			"Scene registration normalizes a Canvas-external Actor to one Transform");
	}

#ifdef NDEBUG
	void TestInvalidGuidRejection()
	{
		const Actor::InitDesc desc(true, TAG_NONE, "Invalid");
		const Guid invalidGuid{};

		Check(ActorFactory::RestoreActor(ActorType::Empty, desc, invalidGuid) == nullptr,
			"RestoreActor rejects an invalid Guid in Release builds");
		Check(ActorFactory::RestoreEmptyActor(desc, invalidGuid) == nullptr,
			"RestoreEmptyActor rejects an invalid Guid in Release builds");
	}
#endif
}

int main()
{
	TestGuidStringRoundTrip();
	TestNewActorGuidGeneration();
	TestNewEmptyActorGuidGeneration();
	TestActorRestoration();
	TestEmptyActorRestoration();
	TestTransformFamilyExclusion();
	TestRectTransformFamilyExclusion();
	TestRegistryUsesTransformFamilyPolicy();
	TestUniqueComponentDuplicateRejection();
	TestRendererFamilyExclusion();
	TestRegistryUsesRendererFamilyPolicy();
	TestRegisteredComponentNamesAreSorted();
	TestSceneRequiresOneTransformFamilyComponent();

#ifdef NDEBUG
	TestInvalidGuidRejection();
#else
	std::cout << "[INFO] Invalid Guid calls are guarded by assert in Debug builds.\n";
#endif

	if (g_failures != 0)
	{
		std::cerr << g_failures << " ActorFactory test(s) failed.\n";
		return 1;
	}

	std::cout << "All ActorFactory tests passed.\n";
	return 0;
}
