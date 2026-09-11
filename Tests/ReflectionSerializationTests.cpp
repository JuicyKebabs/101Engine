#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "nlohmann/json.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace
{
	enum class Mode
	{
		Idle,
		Running,
	};

	struct TestObject
	{
		bool enabled = true;
		std::int32_t signedValue = -12;
		std::uint16_t unsignedValue = 34;
		float floatValue = 1.25f;
		double doubleValue = 2.5;
		std::string name = "Source";
		Vector2 vector2{ 1.0f, 2.0f };
		Vector3 vector3{ 3.0f, 4.0f, 5.0f };
		Vector4 vector4{ 6.0f, 7.0f, 8.0f, 9.0f };
		Vector4 color{ 0.1f, 0.2f, 0.3f, 0.4f };
		Quaternion rotation{ 0.0f, 0.5f, 0.0f, 1.0f };
		Mode mode = Mode::Running;
		std::int32_t hidden = 77;
		std::int32_t readOnly = 88;
		std::int32_t transient = 99;
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

	EnumMetadata BuildModeMetadata()
	{
		return *EnumMetadataBuilder<Mode>()
			.Add("idle", Mode::Idle)
			.Add("running", Mode::Running)
			.Build();
	}

	TypeMetadata BuildTestMetadata()
	{
		const EnumMetadata modeMetadata = BuildModeMetadata();
		TypeMetadataBuilder<TestObject> builder("TestObject");
		builder.Property("vector4", &TestObject::vector4);
		builder.Property("unsigned", &TestObject::unsignedValue);
		builder.Property("transient", &TestObject::transient).Serialization(std::nullopt);
		builder.Property("string", &TestObject::name);
		builder.Property("signed", &TestObject::signedValue);
		builder.Property("rotation", &TestObject::rotation);
		builder.Property("readOnly", &TestObject::readOnly).Inspector(InspectorMetadata{.readOnly = true});
		builder.Property("mode", &TestObject::mode).Enum(modeMetadata);
		builder.Property("hidden", &TestObject::hidden).Inspector(std::nullopt);
		builder.Property("float", &TestObject::floatValue);
		builder.Property("enabled", &TestObject::enabled);
		builder.Property("double", &TestObject::doubleValue);
		builder.Property("color", &TestObject::color).Inspector(InspectorMetadata{.presentation = InspectorPresentation::Color});
		builder.Property("vector3", &TestObject::vector3);
		builder.Property("vector2", &TestObject::vector2);
		return *builder.Build();
	}

	bool Equal(const TestObject& lhs, const TestObject& rhs)
	{
		return lhs.enabled == rhs.enabled &&
			lhs.signedValue == rhs.signedValue &&
			lhs.unsignedValue == rhs.unsignedValue &&
			lhs.floatValue == rhs.floatValue &&
			lhs.doubleValue == rhs.doubleValue &&
			lhs.name == rhs.name &&
			lhs.vector2.x == rhs.vector2.x && lhs.vector2.y == rhs.vector2.y &&
			lhs.vector3.x == rhs.vector3.x && lhs.vector3.y == rhs.vector3.y && lhs.vector3.z == rhs.vector3.z &&
			lhs.vector4.x == rhs.vector4.x && lhs.vector4.y == rhs.vector4.y &&
			lhs.vector4.z == rhs.vector4.z && lhs.vector4.w == rhs.vector4.w &&
			lhs.color.x == rhs.color.x && lhs.color.y == rhs.color.y &&
			lhs.color.z == rhs.color.z && lhs.color.w == rhs.color.w &&
			lhs.rotation.x == rhs.rotation.x && lhs.rotation.y == rhs.rotation.y &&
			lhs.rotation.z == rhs.rotation.z && lhs.rotation.w == rhs.rotation.w &&
			lhs.mode == rhs.mode && lhs.hidden == rhs.hidden && lhs.readOnly == rhs.readOnly;
	}

	void TestRoundTripAndPolicies()
	{
		const TypeMetadata metadata = BuildTestMetadata();
		const TestObject source;
		nlohmann::json serialized;

		Check(ReflectionSerializer::Serialize(metadata, typeid(TestObject), &source, serialized),
			"Serializer accepts every supported logical type");
		Check(serialized.size() == 14 && !serialized.contains("transient"),
			"Serializer includes only Serializable properties");
		Check(serialized["mode"] == "running",
			"Serializer writes enum serialized names");
		Check(serialized["vector2"] == nlohmann::json({ 1.0f, 2.0f }) &&
			serialized["rotation"] == nlohmann::json({ 0.0f, 0.5f, 0.0f, 1.0f }),
			"Serializer writes fixed component arrays");

		TestObject restored;
		restored.enabled = false;
		restored.hidden = 0;
		restored.readOnly = 0;
		restored.transient = -1;
		Check(ReflectionDeserializer::Deserialize(metadata, typeid(TestObject), serialized, &restored),
			"Deserializer restores a complete property object");
		Check(Equal(source, restored),
			"Every supported Serializable property round trips");
		Check(restored.transient == -1,
			"Non-Serializable property is not changed");
	}

	void TestDeterministicOrder()
	{
		const TypeMetadata metadata = BuildTestMetadata();
		const TestObject source;
		nlohmann::json first;
		nlohmann::json second;
		ReflectionSerializer::Serialize(metadata, typeid(TestObject), &source, first);
		ReflectionSerializer::Serialize(metadata, typeid(TestObject), &source, second);

		Check(first.dump() == second.dump(), "Repeated serialization is deterministic");

		std::string previous;
		bool sorted = true;
		for (auto entry = first.begin(); entry != first.end(); ++entry)
		{
			if (!previous.empty() && previous >= entry.key()) sorted = false;
			previous = entry.key();
		}
		Check(sorted, "Serialized properties use serialized-name order");
	}

	void TestNestedObjectScopes()
	{
		struct NestedObject
		{
			Vector3 rigRotation{ 1.0f, 2.0f, 3.0f };
			Vector3 poseRotation{ 4.0f, 5.0f, 6.0f };
		};

		TypeMetadataBuilder<NestedObject> builder("NestedObject");
		builder
			.Object("rig", [](auto& rig)
			{
				rig.Property("rotation", &NestedObject::rigRotation);
			})
			.Object("pose", [](auto& pose)
			{
				pose.Property("rotation", &NestedObject::poseRotation);
			});
		const TypeMetadata metadata = *builder.Build();

		NestedObject source;
		nlohmann::json json;
		Check(ReflectionSerializer::Serialize(
			metadata, typeid(NestedObject), &source, json) &&
			json == nlohmann::json({
				{ "pose", { { "rotation", { 4.0f, 5.0f, 6.0f } } } },
				{ "rig", { { "rotation", { 1.0f, 2.0f, 3.0f } } } }
			}),
			"Object scopes serialize leaf properties into nested JSON objects");

		NestedObject restored;
		restored.rigRotation = {};
		restored.poseRotation = {};
		Check(ReflectionDeserializer::Deserialize(
			metadata, typeid(NestedObject), json, &restored) &&
			restored.rigRotation.x == 1.0f && restored.poseRotation.x == 4.0f,
			"Nested property paths deserialize into their independent leaves");

		nlohmann::json unknown = json;
		unknown["rig"]["unknown"] = 1;
		Check(!ReflectionDeserializer::Deserialize(
			metadata, typeid(NestedObject), unknown, &restored),
			"Nested schema rejects unknown object members");

		nlohmann::json missing = json;
		missing["pose"].erase("rotation");
		Check(!ReflectionDeserializer::Deserialize(
			metadata, typeid(NestedObject), missing, &restored),
			"Nested schema rejects missing leaf properties");

		nlohmann::json objectMismatch = json;
		objectMismatch["rig"] = nlohmann::json::array();
		Check(!ReflectionDeserializer::Deserialize(
			metadata, typeid(NestedObject), objectMismatch, &restored),
			"Nested schema rejects an array where an object scope is required");
	}

	void TestIntegerEnumFormat()
	{
		const EnumMetadata modeMetadata = BuildModeMetadata();
		TypeMetadataBuilder<TestObject> metadataBuilder("IntegerEnum");
		metadataBuilder.Property("mode", &TestObject::mode).Enum(modeMetadata).SerializedAs(EnumSerializationFormat::Integer);
		const TypeMetadata metadata = *metadataBuilder.Build();

		TestObject source;
		nlohmann::json json;
		Check(ReflectionSerializer::Serialize(
			metadata, typeid(TestObject), &source, json) && json["mode"] == 1,
			"Integer enum format preserves the registered numeric value");

		TestObject restored;
		restored.mode = Mode::Idle;
		Check(ReflectionDeserializer::Deserialize(
			metadata, typeid(TestObject), json, &restored) && restored.mode == Mode::Running,
			"Integer enum format restores a registered numeric value");

		auto Rejects = [&](nlohmann::json value, const std::string& name)
		{
			TestObject object;
			return Check(!ReflectionDeserializer::Deserialize(
				metadata, typeid(TestObject), { { "mode", std::move(value) } }, &object), name);
		};
		Rejects("running", "Integer enum format rejects a serialized name");
		Rejects(99, "Integer enum format rejects an unregistered integer");
		Rejects(1.0, "Integer enum format rejects a floating-point value");

		TypeMetadataBuilder<TestObject> nameMetadataBuilder("NameEnum");
		nameMetadataBuilder.Property("mode", &TestObject::mode).Enum(modeMetadata);
		const TypeMetadata nameMetadata = *nameMetadataBuilder.Build();
		TestObject object;
		Check(!ReflectionDeserializer::Deserialize(
			nameMetadata, typeid(TestObject), { { "mode", 1 } }, &object),
			"Name enum format continues to reject integer input");
	}

	void TestInvalidInputs()
	{
		const TypeMetadata metadata = BuildTestMetadata();
		const TestObject source;
		nlohmann::json valid;
		ReflectionSerializer::Serialize(metadata, typeid(TestObject), &source, valid);

		auto Rejects = [&](nlohmann::json invalid, const std::string& name)
		{
			TestObject object;
			return Check(!ReflectionDeserializer::Deserialize(metadata, typeid(TestObject), invalid, &object), name);
		};

		nlohmann::json unknown = valid;
		unknown["unknown"] = 1;
		Rejects(unknown, "Deserializer rejects unknown fields");

		nlohmann::json missing = valid;
		missing.erase("string");
		Rejects(missing, "Deserializer rejects missing Serializable properties");

		nlohmann::json wrongType = valid;
		wrongType["enabled"] = 1;
		Rejects(wrongType, "Deserializer rejects scalar type mismatches");

		nlohmann::json implicitNumericConversion = valid;
		implicitNumericConversion["float"] = 1;
		Rejects(implicitNumericConversion, "Deserializer rejects integer-to-float conversion");

		nlohmann::json roundedInteger = valid;
		roundedInteger["signed"] = 1.5;
		Rejects(roundedInteger, "Deserializer rejects floating-point-to-integer conversion");

		nlohmann::json negativeUnsigned = valid;
		negativeUnsigned["unsigned"] = -1;
		Rejects(negativeUnsigned, "Deserializer rejects negative unsigned integers");

		nlohmann::json outOfRange = valid;
		outOfRange["unsigned"] = 100000;
		Rejects(outOfRange, "Deserializer rejects destination integer overflow");

		nlohmann::json wrongLength = valid;
		wrongLength["vector3"] = { 1.0, 2.0 };
		Rejects(wrongLength, "Deserializer rejects incorrect fixed-array lengths");

		nlohmann::json wrongComponentType = valid;
		wrongComponentType["vector2"] = { 1, 2.0 };
		Rejects(wrongComponentType, "Deserializer rejects array component type mismatches");

		nlohmann::json nonFinite = valid;
		nonFinite["double"] = std::numeric_limits<double>::infinity();
		Rejects(nonFinite, "Deserializer rejects non-finite numbers");

		nlohmann::json unknownEnum = valid;
		unknownEnum["mode"] = "missing";
		Rejects(unknownEnum, "Deserializer rejects unregistered enum names");

		nlohmann::json numericEnum = valid;
		numericEnum["mode"] = 1;
		Rejects(numericEnum, "Deserializer rejects numeric enum values");

		Rejects(nlohmann::json::array(), "Deserializer requires a JSON object");
	}

	void TestValidationPrecedesWrites()
	{
		struct Tracked
		{
			std::int32_t first = 1;
			std::int32_t second = 2;
			int writes = 0;
		};

		TypeMetadataBuilder<Tracked> metadataBuilder("Tracked");
		metadataBuilder.Accessor<std::int32_t>("first", [](const Tracked& object, std::int32_t& value) { value = object.first; return true; }, [](Tracked& object, const std::int32_t& value) { ++object.writes; object.first = value; return true; });
		metadataBuilder.Accessor<std::int32_t>("second", [](const Tracked& object, std::int32_t& value) { value = object.second; return true; }, [](Tracked& object, const std::int32_t& value) { ++object.writes; object.second = value; return false; });
		const TypeMetadata metadata = *metadataBuilder.Build();

		Tracked object;
		const nlohmann::json invalid = { { "first", 10 }, { "second", "invalid" } };
		Check(!ReflectionDeserializer::Deserialize(metadata, typeid(Tracked), invalid, &object) &&
			object.writes == 0 && object.first == 1 && object.second == 2,
			"Schema validation failure performs no writes");

		const nlohmann::json outOfRange = {
			{ "first", static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1 },
			{ "second", 20 }
		};
		Check(!ReflectionDeserializer::Deserialize(metadata, typeid(Tracked), outOfRange, &object) &&
			object.writes == 0 && object.first == 1 && object.second == 2,
			"Destination range validation performs no writes");

		const nlohmann::json writeFailure = { { "first", 10 }, { "second", 20 } };
		ReflectionError error;
		const bool rejected = !ReflectionDeserializer::Deserialize(
			metadata, typeid(Tracked), writeFailure, &object, {}, &error);
		const bool stateRestored = object.first == 1 && object.second == 2;
		const bool errorClassified = error.code == ReflectionErrorCode::RollbackFailed;
		Check(rejected && stateRestored && errorClassified,
			"Write callback failure restores the original object and reports its category");
	}

	void TestNonFiniteSerialization()
	{
		const TypeMetadata metadata = BuildTestMetadata();
		TestObject source;
		source.floatValue = std::numeric_limits<float>::quiet_NaN();
		nlohmann::json output = { { "unchanged", true } };
		Check(!ReflectionSerializer::Serialize(metadata, typeid(TestObject), &source, output),
			"Serializer rejects non-finite property values");
		Check(output == nlohmann::json({ { "unchanged", true } }),
			"Failed serialization does not publish a partial object");
	}

	void TestObjectTypeValidation()
	{
		struct OtherObject {};

		const TypeMetadata metadata = BuildTestMetadata();
		TestObject object;
		nlohmann::json serialized;
		ReflectionSerializer::Serialize(metadata, typeid(TestObject), &object, serialized);

		nlohmann::json output = { { "unchanged", true } };
		Check(!ReflectionSerializer::Serialize(metadata, typeid(OtherObject), &object, output) &&
			output == nlohmann::json({ { "unchanged", true } }),
			"Serializer rejects a mismatched object type");

		const TestObject before = object;
		Check(!ReflectionDeserializer::Deserialize(
			metadata, typeid(OtherObject), serialized, &object) && Equal(before, object),
			"Deserializer rejects a mismatched object type without mutation");
	}

	void TestOptionalPropertyRequirement()
	{
		struct CompatibleObject
		{
			std::int32_t required = 1;
			std::int32_t optional = 7;
		};

		TypeMetadataBuilder<CompatibleObject> metadataBuilder("CompatibleObject");
		metadataBuilder.Property("required", &CompatibleObject::required);
		metadataBuilder.Property("optional", &CompatibleObject::optional).Optional();
		const TypeMetadata metadata = *metadataBuilder.Build();
		CompatibleObject object;

		Check(metadata.FindProperty("required")->GetSerializationMetadata()->requirement == PropertyRequirement::Required,
			"Property requirement defaults to Required");
		Check(ReflectionDeserializer::Deserialize(
			metadata, typeid(CompatibleObject), nlohmann::json({ { "required", 5 } }), &object) &&
			object.required == 5 && object.optional == 7,
			"A missing Optional property keeps the destination default");
		Check(!ReflectionDeserializer::Deserialize(
			metadata, typeid(CompatibleObject), nlohmann::json({ { "optional", 9 } }), &object),
			"A missing Required property is rejected");
	}
}

int main()
{
	TestRoundTripAndPolicies();
	TestDeterministicOrder();
	TestNestedObjectScopes();
	TestIntegerEnumFormat();
	TestInvalidInputs();
	TestValidationPrecedesWrites();
	TestNonFiniteSerialization();
	TestObjectTypeValidation();
	TestOptionalPropertyRequirement();

	if (g_failures == 0)
	{
		std::cout << "All ReflectionSerialization tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " ReflectionSerialization test(s) failed.\n";
	return 1;
}
