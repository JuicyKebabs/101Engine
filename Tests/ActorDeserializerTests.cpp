#include "Engine/Actor/ActorFactory.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Scene/ActorDeserializer.h"
#include "Engine/Scene/ActorSerializer.h"
#include "Engine/Scene/SceneBase.h"

#include <iostream>
#include <string>

namespace
{
	using json = nlohmann::json;

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

	void TestDeserializeCreatesDetachedActor()
	{
		SceneBase scene;
		Actor* source = scene.AddRootActor(
			ActorFactory::CreateEmptyActor(
				Actor::InitDesc(false, TAG_NONE, "Detached")));
		source->GetComponentByClass<Transform>()->SetLocalPosition({ 1.0f, 2.0f, 3.0f });

		json actorRecord;
		Check(ActorSerializer::SerializeActorRecord(source, &scene, actorRecord),
			"ActorSerializer creates input for ActorDeserializer");

		std::unique_ptr<Actor> restored =
			ActorDeserializer::DeserializeActorRecord(
				actorRecord,
				source->GetGuid());

		Check(restored != nullptr,
			"ActorDeserializer accepts a valid Actor record");
		Check(restored && restored->GetGuid() == source->GetGuid(),
			"ActorDeserializer preserves the supplied Guid");
		Check(restored && restored->GetName() == "Detached" && !restored->IsActive(),
			"ActorDeserializer restores basic Actor properties");
		Check(restored && restored->GetOwner() == nullptr && restored->GetHandle().IsNull(),
			"Deserialized Actor remains detached from every Scene");

		Transform* transform = restored
			? restored->GetComponentByClass<Transform>()
			: nullptr;
		const Vector3 position = transform
			? transform->GetLocalPosition()
			: Vector3::Zero();
		Check(transform &&
			position.x == 1.0f &&
			position.y == 2.0f &&
			position.z == 3.0f,
			"ActorDeserializer restores serialized component data");
	}

	void TestDeserializeRejectsInvalidRecords()
	{
		const Guid guid = GuidGenerator::Generate();

		json missingComponents = {
			{ "name", "Invalid" },
			{ "is_active", true },
			{ "tag", "None" }
		};

		Check(!ActorDeserializer::DeserializeActorRecord(missingComponents, guid),
			"ActorDeserializer rejects a record without components");

		json noTransform = {
			{ "name", "Invalid" },
			{ "is_active", true },
			{ "tag", "None" },
			{ "components", json::array() }
		};

		Check(!ActorDeserializer::DeserializeActorRecord(noTransform, guid),
			"ActorDeserializer requires one Transform-family component");
		Check(!ActorDeserializer::DeserializeActorRecord(noTransform, Guid{}),
			"ActorDeserializer rejects an invalid Guid");
	}
}

int main()
{
	TestDeserializeCreatesDetachedActor();
	TestDeserializeRejectsInvalidRecords();

	if (g_failures == 0)
	{
		std::cout << "All ActorDeserializer tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " ActorDeserializer test(s) failed.\n";
	return 1;
}
