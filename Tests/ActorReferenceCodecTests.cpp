#include "Engine/Actor/ActorFactory.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/ActorReferenceCodec.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "Engine/Scene/SceneActorReferenceContext.h"
#include "Engine/Scene/SceneBase.h"
#include "nlohmann/json.hpp"

#include <iostream>
#include <string>

namespace
{
	struct ReferenceObject
	{
		ActorReference target;
	};

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

	std::unique_ptr<Actor> MakeActor(const char* name)
	{
		return ActorFactory::CreateEmptyActor(Actor::InitDesc(true, TAG_NONE, name));
	}

	TypeMetadata BuildMetadata()
	{
		return *TypeMetadataBuilder<ReferenceObject>("ReferenceObject")
			.AddMember("target", &ReferenceObject::target)
			.Build();
	}

	void TestNullReferenceRoundTrip()
	{
		SceneBase scene;
		GuidActorReferenceCodec codec;
		SceneActorReferenceContext context(scene);
		const TypeMetadata metadata = BuildMetadata();
		ReferenceObject source;
		nlohmann::json serialized;

		Check(ReflectionSerializer::Serialize(
			metadata, typeid(ReferenceObject), &source, serialized, { &codec, &context }) &&
			serialized["target"].is_null(),
			"Null ActorReference serializes to null");

		ReferenceObject restored;
		Check(ReflectionDeserializer::Deserialize(
			metadata, typeid(ReferenceObject), serialized, &restored, { &codec, &context }) &&
			!restored.target.HasValue(),
			"Null deserializes to an empty ActorReference");
	}

	void TestGuidSerializationAndDelayedResolution()
	{
		SceneBase sourceScene;
		Actor* target = sourceScene.AddRootActor(MakeActor("Target"));
		const Guid targetGuid = target->GetGuid();
		GuidActorReferenceCodec codec;
		SceneActorReferenceContext sourceContext(sourceScene);

		const TypeMetadata metadata = BuildMetadata();
		ReferenceObject source;
		source.target.Set(target);
		nlohmann::json serialized;
		Check(ReflectionSerializer::Serialize(
			metadata, typeid(ReferenceObject), &source, serialized, { &codec, &sourceContext }) &&
			serialized["target"] == targetGuid.ToString(),
			"Valid ActorReference serializes to its Actor Guid");

		SceneBase restoredScene;
		SceneActorReferenceContext restoredContext(restoredScene);
		ReferenceObject restored;
		Check(ReflectionDeserializer::Deserialize(
			metadata, typeid(ReferenceObject), serialized, &restored, { &codec, &restoredContext }) &&
			restored.target.GetGuid() == targetGuid &&
			restored.target.Resolve(restoredScene) == nullptr,
			"Deserialize preserves an unresolved Actor Guid");
		Check(codec.Resolve(restored.target, restoredContext) == ActorReferenceCodecResult::ActorNotFound,
			"Missing Actor is reported before registration");

		Actor* restoredTarget = restoredScene.AddRootActor(
			ActorFactory::RestoreEmptyActor(
				Actor::InitDesc(true, TAG_NONE, "RestoredTarget"), targetGuid));
		Check(codec.Resolve(restored.target, restoredContext) == ActorReferenceCodecResult::Success &&
			restored.target.Resolve(restoredScene) == restoredTarget,
			"Explicit resolution connects the reference after registration");
	}

	void TestFailureResults()
	{
		SceneBase scene;
		GuidActorReferenceCodec codec;
		SceneActorReferenceContext context(scene);
		ActorReference reference;

		nlohmann::json output;
		Check(codec.Deserialize(42, reference) == ActorReferenceCodecResult::InvalidJsonType,
			"Non-string ActorReference JSON reports InvalidJsonType");
		Check(codec.Deserialize("not-a-guid", reference) == ActorReferenceCodecResult::InvalidGuid,
			"Malformed Guid reports InvalidGuid");

		const Guid missingGuid = GuidGenerator::Generate();
		reference.SetGuid(missingGuid);
		Check(codec.Serialize(reference, context, output) == ActorReferenceCodecResult::ActorNotFound &&
			codec.Resolve(reference, context) == ActorReferenceCodecResult::ActorNotFound,
			"Unknown Guid reports ActorNotFound");

		SceneBase otherScene;
		Actor* otherActor = otherScene.AddRootActor(MakeActor("OtherScene"));
		reference.Set(otherActor);
		Check(codec.Serialize(reference, context, output) == ActorReferenceCodecResult::ActorNotFound,
			"Cross-Scene reference is treated as ActorNotFound");

		Actor* pending = scene.AddRootActor(MakeActor("Pending"));
		reference.Set(pending);
		scene.RemoveActor(pending);
		Check(codec.Serialize(reference, context, output) == ActorReferenceCodecResult::PendingDestroy &&
			codec.Resolve(reference, context) == ActorReferenceCodecResult::PendingDestroy,
			"Actor pending destruction reports PendingDestroy");
	}
}

int main()
{
	TestNullReferenceRoundTrip();
	TestGuidSerializationAndDelayedResolution();
	TestFailureResults();

	if (g_failures == 0)
	{
		std::cout << "All ActorReferenceCodec tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " ActorReferenceCodec test(s) failed.\n";
	return 1;
}
