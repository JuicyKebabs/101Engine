#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Camera.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/GUID/GuidGenerator.h"

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
