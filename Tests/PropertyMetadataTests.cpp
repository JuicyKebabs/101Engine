#include "Engine/Core/Reflection/PropertyMetadata.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>

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

	enum class TestMode
	{
		Idle = 1,
		Active = 2,
	};

	struct TestObject
	{
		bool enabled = true;
		std::int32_t count = 4;
		std::uint16_t mask = 7;
		float speed = 1.5f;
		double precision = 2.25;
		std::string label = "test";
		Vector2 uv{ 1.0f, 2.0f };
		Vector3 position{ 3.0f, 4.0f, 5.0f };
		Vector4 direction{ 6.0f, 7.0f, 8.0f, 9.0f };
		Vector4 tint{ 0.1f, 0.2f, 0.3f, 1.0f };
		Quaternion rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
		TestMode mode = TestMode::Idle;
		int validatedValue = 0;
		int setterCallCount = 0;

		bool SetValidatedValue(int value)
		{
			++setterCallCount;
			if (value < 0) return false;
			validatedValue = value;
			return true;
		}
	};

	struct UnsupportedValue
	{
		int value = 0;
	};

	struct UnsupportedObject
	{
		UnsupportedValue value;
	};

	struct OtherObject
	{
		int value = 0;
	};

	std::optional<EnumMetadata> BuildTestEnum()
	{
		return EnumMetadataBuilder<TestMode>()
			.Add("idle", TestMode::Idle)
			.Add("active", TestMode::Active)
			.Build();
	}

	std::optional<TypeMetadata> BuildTestMetadata()
	{
		const auto enumMetadata = BuildTestEnum();
		if (!enumMetadata) return std::nullopt;

		TypeMetadataBuilder<TestObject> builder("TestObject");
		builder
			.AddMember("enabled", &TestObject::enabled)
			.AddMember("count", &TestObject::count)
			.AddMember("mask", &TestObject::mask)
			.AddMember("speed", &TestObject::speed)
			.AddMember("precision", &TestObject::precision)
			.AddMember("label", &TestObject::label)
			.AddMember("uv", &TestObject::uv)
			.AddMember("position", &TestObject::position)
			.AddMember("direction", &TestObject::direction)
			.AddMember(
				"tint",
				&TestObject::tint,
				DefaultPropertyPolicy(),
				PropertyLogicalType::Color)
			.AddMember("rotation", &TestObject::rotation)
			.AddEnumMember("mode", &TestObject::mode, *enumMetadata)
			.AddAccessorProperty<Vector3>(
				"callback_position",
				PropertyLogicalType::Vector3,
				DefaultPropertyPolicy(),
				[](const TestObject& object, Vector3& outValue)
				{
					outValue = object.position;
					return true;
				},
				[](TestObject& object, const Vector3& value)
				{
					object.position = value;
					return true;
				})
			.AddAccessorProperty<int>(
				"validated_value",
				PropertyLogicalType::SignedInteger,
				PropertyPolicy::Serializable |
					PropertyPolicy::Inspectable |
					PropertyPolicy::EditorReadOnly,
				[](const TestObject& object, int& outValue)
				{
					outValue = object.validatedValue;
					return true;
				},
				[](TestObject& object, const int& value)
				{
					return object.SetValidatedValue(value);
				});

		return builder.Build();
	}

	void TestMetadataEnumerationAndLookup()
	{
		const auto metadata = BuildTestMetadata();
		Check(metadata.has_value(), "Valid type metadata builds");
		if (!metadata) return;

		Check(metadata->GetType() == std::type_index(typeid(TestObject)),
			"Type metadata retains the represented C++ type");
		Check(metadata->GetProperties().size() == 14,
			"Completed metadata enumerates every registered property");
		static_assert(std::is_same_v<
			decltype(std::declval<const TypeMetadata&>().GetProperties()),
			const std::vector<PropertyMetadata>&>);

		const PropertyMetadata* tint = metadata->FindProperty("tint");
		Check(tint && tint->GetLogicalType() == PropertyLogicalType::Color,
			"Color remains logically distinct from Vector4");
		Check(tint && tint->GetPath().ToString() == "/tint",
			"Top-level properties receive a normalized absolute path");

		const PropertyMetadata* readOnly = metadata->FindProperty("validated_value");
		Check(readOnly &&
			HasPropertyPolicy(readOnly->GetPolicy(), PropertyPolicy::Serializable) &&
			HasPropertyPolicy(readOnly->GetPolicy(), PropertyPolicy::Inspectable) &&
			HasPropertyPolicy(readOnly->GetPolicy(), PropertyPolicy::EditorReadOnly),
			"Property policy is available from completed metadata");
		Check(metadata->FindProperty("missing") == nullptr,
			"Unknown serialized property name is not found");
	}

	void TestPropertyPathsAndObjectScopes()
	{
		const auto escaped = PropertyPath::FromString("/material~1slot/value~0name");
		Check(escaped && escaped->GetMembers().size() == 2 &&
			escaped->GetMembers()[0] == "material/slot" &&
			escaped->GetMembers()[1] == "value~name" &&
			escaped->ToString() == "/material~1slot/value~0name",
			"PropertyPath validates and normalizes JSON Pointer escaping");
		Check(!PropertyPath::FromString("rig/value") &&
			!PropertyPath::FromString("/rig/~2value") &&
			!PropertyPath::FromString("/rig/"),
			"Invalid JSON Pointers are rejected");

		TypeMetadataBuilder<TestObject> nestedBuilder("NestedObject");
		nestedBuilder
			.Object("rig", [](auto& rig)
			{
				rig.template AddAccessor<Vector3>(
					"rotation", PropertyLogicalType::Vector3, DefaultPropertyPolicy(),
					[](const TestObject& object, Vector3& value) { value = object.position; return true; },
					[](TestObject& object, const Vector3& value) { object.position = value; return true; });
			})
			.Object("pose", [](auto& pose)
			{
				pose.template AddAccessor<Vector3>(
					"rotation", PropertyLogicalType::Vector3, DefaultPropertyPolicy(),
					[](const TestObject& object, Vector3& value)
					{
						value = { object.direction.x, object.direction.y, object.direction.z };
						return true;
					},
					[](TestObject& object, const Vector3& value) { object.direction = Vector4(value.x, value.y, value.z, 0.0f); return true; });
			});
		const auto nested = nestedBuilder.Build();
		Check(nested && nested->GetProperties().size() == 2,
			"Object scopes register leaf properties without recursive struct reflection");
		const auto rigPath = PropertyPath::FromString("/rig/rotation");
		const auto posePath = PropertyPath::FromString("/pose/rotation");
		Check(nested && rigPath && posePath &&
			nested->FindPropertyByPath(*rigPath) &&
			nested->FindPropertyByPath(*posePath),
			"Identical leaf names remain distinct through their complete paths");

		TypeMetadataBuilder<TestObject> duplicateLeaf("DuplicateLeaf");
		duplicateLeaf.Object("rig", [](auto& rig)
		{
			rig.AddMember("value", &TestObject::count)
				.AddMember("value", &TestObject::speed);
		});
		Check(!duplicateLeaf.Build(), "Duplicate paths in one object are rejected");

		TypeMetadataBuilder<TestObject> propertyObjectCollision("Collision");
		propertyObjectCollision.AddMember("rig", &TestObject::count).Object("rig", [](auto& rig)
		{
			rig.AddMember("value", &TestObject::speed);
		});
		Check(!propertyObjectCollision.Build(),
			"A property cannot also be an ancestor object path");

		TypeMetadataBuilder<TestObject> emptyObject("EmptyObject");
		emptyObject.Object("rig", [](auto&) {});
		Check(!emptyObject.Build(), "An object scope without leaf properties is rejected");

		TypeMetadataBuilder<TestObject> duplicateObject("DuplicateObject");
		duplicateObject.Object("rig", [](auto& rig) { rig.AddMember("a", &TestObject::count); });
		duplicateObject.Object("rig", [](auto& rig) { rig.AddMember("b", &TestObject::speed); });
		Check(!duplicateObject.Build(), "Duplicate object scope registration is rejected");
	}

	void TestMemberAndCallbackOperations()
	{
		const auto metadata = BuildTestMetadata();
		if (!metadata) return;

		TestObject object;
		const PropertyMetadata* count = metadata->FindProperty("count");
		PropertyValue value = false;
		Check(count && count->Read(typeid(TestObject), &object, value) &&
			std::get<std::int64_t>(value) == 4,
			"Member pointer read converts a signed integer to PropertyValue");
		Check(count && count->Write(typeid(TestObject), &object, std::int64_t{ 12 }) &&
			object.count == 12,
			"Member pointer write converts PropertyValue to the member type");

		const PropertyMetadata* validated = metadata->FindProperty("validated_value");
		Check(validated && validated->Write(
			typeid(TestObject), &object, std::int64_t{ 9 }) &&
			object.validatedValue == 9 && object.setterCallCount == 1,
			"Callback write uses the registered setter even for EditorReadOnly properties");
		Check(validated && !validated->Write(
			typeid(TestObject), &object, std::int64_t{ -1 }) &&
			object.validatedValue == 9 && object.setterCallCount == 2,
			"Callback write reports setter validation failure");

		const PropertyMetadata* callbackPosition = metadata->FindProperty("callback_position");
		const Vector3 newPosition{ 10.0f, 11.0f, 12.0f };
		Check(callbackPosition && callbackPosition->Write(
			typeid(TestObject), &object, newPosition) &&
			object.position.x == 10.0f &&
			object.position.y == 11.0f &&
			object.position.z == 12.0f,
			"Callback registration supports an Engine standard value type");
	}

	void TestTypeAndValueMismatchDoNotMutate()
	{
		const auto metadata = BuildTestMetadata();
		if (!metadata) return;

		TestObject object;
		OtherObject other;
		const PropertyMetadata* count = metadata->FindProperty("count");
		PropertyValue output = std::string("unchanged");

		Check(count && !count->Read(typeid(OtherObject), &other, output) &&
			std::get<std::string>(output) == "unchanged",
			"Read rejects the wrong object type without changing output");
		Check(count && !count->Write(typeid(OtherObject), &other, std::int64_t{ 8 }) &&
			other.value == 0,
			"Write rejects the wrong object type without changing it");
		Check(count && !count->Write(typeid(TestObject), &object, std::string("wrong")) &&
			object.count == 4,
			"Write rejects the wrong PropertyValue alternative without mutation");
		Check(count && !count->Write(typeid(TestObject), &object,
			static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1) &&
			object.count == 4,
			"Write rejects an integer outside the member value range");
	}

	void TestEnumMetadataAndOperations()
	{
		const auto enumMetadata = BuildTestEnum();
		Check(enumMetadata && enumMetadata->GetEntries().size() == 2,
			"Valid explicit enum metadata builds");
		Check(enumMetadata && enumMetadata->FindByName("active") &&
			enumMetadata->FindByName("active")->value == 2,
			"Enum metadata supports serialized-name lookup");
		Check(enumMetadata && enumMetadata->FindByValue(1) &&
			enumMetadata->FindByValue(1)->serializedName == "idle",
			"Enum metadata supports value lookup");

		Check(!EnumMetadataBuilder<TestMode>()
			.Add("", TestMode::Idle).Build(),
			"Empty enum serialized names are rejected");
		Check(!EnumMetadataBuilder<TestMode>()
			.Add("same", TestMode::Idle)
			.Add("same", TestMode::Active).Build(),
			"Duplicate enum serialized names are rejected");
		Check(!EnumMetadataBuilder<TestMode>()
			.Add("idle", TestMode::Idle)
			.Add("also_idle", TestMode::Idle).Build(),
			"Duplicate enum values are rejected");

		const auto metadata = BuildTestMetadata();
		if (!metadata) return;

		TestObject object;
		const PropertyMetadata* mode = metadata->FindProperty("mode");
		PropertyValue value = false;
		Check(mode && mode->Read(typeid(TestObject), &object, value) &&
			std::get<EnumPropertyValue>(value).value == 1,
			"Enum property reads through the fixed PropertyValue variant");
		Check(mode && mode->Write(
			typeid(TestObject), &object, EnumPropertyValue{ 2 }) &&
			object.mode == TestMode::Active,
			"Registered enum value writes to the C++ enum");
		Check(mode && !mode->Write(
			typeid(TestObject), &object, EnumPropertyValue{ 99 }) &&
			object.mode == TestMode::Active,
			"Unregistered enum value is rejected without mutation");
		Check(mode && mode->GetEnumSerializationFormat() == EnumSerializationFormat::Name,
			"Enum name serialization is the default metadata format");
	}

	void TestInvalidTypeMetadataIsNeverPublished()
	{
		TypeMetadataBuilder<TestObject> emptyName("TestObject");
		emptyName.AddMember("", &TestObject::count);
		Check(!emptyName.Build(), "Empty property serialized name rejects the type metadata");

		TypeMetadataBuilder<TestObject> duplicate("TestObject");
		duplicate
			.AddMember("value", &TestObject::count)
			.AddMember("value", &TestObject::speed);
		Check(!duplicate.Build(), "Duplicate property serialized name rejects the type metadata");

		TypeMetadataBuilder<TestObject> incomplete("TestObject");
		incomplete.AddAccessorProperty<int>(
			"value",
			PropertyLogicalType::SignedInteger,
			DefaultPropertyPolicy(),
			{},
			[](TestObject&, const int&) { return true; });
		Check(!incomplete.Build(), "Incomplete read and write operations reject the type metadata");

		TypeMetadataBuilder<TestObject> unsupported("TestObject");
		unsupported.AddMember(
			"value",
			&TestObject::count,
			DefaultPropertyPolicy(),
			PropertyLogicalType::Invalid);
		Check(!unsupported.Build(), "Unsupported logical type rejects the type metadata");

		TypeMetadataBuilder<TestObject> mismatched("TestObject");
		mismatched.AddMember(
			"value",
			&TestObject::count,
			DefaultPropertyPolicy(),
			PropertyLogicalType::Vector3);
		Check(!mismatched.Build(), "Logical and C++ value type mismatch rejects the type metadata");

		TypeMetadataBuilder<TestObject> missingEnum("TestObject");
		missingEnum.AddMember("mode", &TestObject::mode);
		Check(!missingEnum.Build(), "Enum property without explicit enum metadata is rejected");

		TypeMetadataBuilder<TestObject> formatOnInteger("TestObject");
		formatOnInteger.AddAccessorProperty<int>(
			"value", PropertyLogicalType::SignedInteger, DefaultPropertyPolicy(),
			[](const TestObject& object, int& value) { value = object.count; return true; },
			[](TestObject& object, const int& value) { object.count = value; return true; },
			std::nullopt, EnumSerializationFormat::Integer);
		Check(!formatOnInteger.Build(),
			"Enum serialization format on a non-enum property is rejected");

		TypeMetadataBuilder<TestObject> noProperties("EmptyTestObject");
		Check(noProperties.Build().has_value(), "A type with no registered properties can be completed");

		TypeMetadataBuilder<UnsupportedObject> unsupportedCppType("UnsupportedObject");
		unsupportedCppType.AddMember("value", &UnsupportedObject::value);
		Check(!unsupportedCppType.Build(), "Unsupported C++ value type rejects the type metadata");
	}
}

int main()
{
	TestMetadataEnumerationAndLookup();
	TestPropertyPathsAndObjectScopes();
	TestMemberAndCallbackOperations();
	TestTypeAndValueMismatchDoNotMutate();
	TestEnumMetadataAndOperations();
	TestInvalidTypeMetadataIsNeverPublished();

	if (g_failures != 0)
	{
		std::cerr << g_failures << " PropertyMetadata test(s) failed.\n";
		return 1;
	}

	std::cout << "All PropertyMetadata tests passed.\n";
	return 0;
}
