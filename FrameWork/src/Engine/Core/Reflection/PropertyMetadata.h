#pragma once
#include "Engine/Actor/ActorReference.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Reflection/PropertyPath.h"
#include "Engine/Resource/AssetReference.h"
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

//---------------------------------------------------------------------------------------------------------------------------------
// Type and property metadata system.
// PropertyMetadata owns property identity and type-erased accessors. Optional serialization
// and Inspector facets describe how each consumer uses that property. TypeMetadata owns
// validation rules that apply to the complete object rather than to one property.
//---------------------------------------------------------------------------------------------------------------------------------

// Static list of logical types for properties.
// This expresses the type of the type-erased property value for serialization, inspection, and editing purposes.
// The logical type is used to determine how to treat the property value in a generic way, without knowing the concrete type.
enum class PropertyLogicalType
{
	Invalid,
	Bool,
	SignedInteger,
	UnsignedInteger,
	Float,
	Double,
	String,
	Vector2,
	Vector3,
	Vector4,
	Quaternion,
	Enum,
	ActorReference,
	AssetReference,
};

// Format setting of how enum values are serialized and deserialized.
enum class EnumSerializationFormat
{
	Name,
	Integer,
};

// Requirement setting of how a property is treated in deserialization
enum class PropertyRequirement
{
	Required,	// Need to be present in the serialized data, otherwise deserialization will fail.
	Optional,	// Not necessary to be present in the serialized data,
};

enum class ReflectionErrorCode
{
	None,
	InvalidObject,
	InvalidMetadata,
	SchemaMismatch,
	MissingProperty,
	InvalidPropertyValue,
	PropertyReadFailed,
	PropertyWriteFailed,
	TypeInvariantViolation,
	RollbackFailed,
};

struct ReflectionError
{
	ReflectionErrorCode code = ReflectionErrorCode::None;
	std::optional<PropertyPath> path;
	std::string message;
};

enum class NumericUnit
{
	None,
	Radians,
	Degrees,
};

struct NumericEditorMetadata
{
	float dragSpeed = 0.05f;
	std::optional<double> minimum;
	std::optional<double> maximum;
	NumericUnit storageUnit = NumericUnit::None;
	NumericUnit displayUnit = NumericUnit::None;
};

enum class InspectorPresentation { Default, Color };

struct InspectorMetadata
{
	std::string label;
	InspectorPresentation presentation = InspectorPresentation::Default;
	bool readOnly = false;
	std::optional<NumericEditorMetadata> numeric;
};

struct SerializationMetadata
{
	PropertyRequirement requirement = PropertyRequirement::Required;
	std::optional<EnumSerializationFormat> enumFormat;
};

// Stores the runtime value of an enum property. 
// This is used to represent enum values in a type-erased way,
struct EnumPropertyValue
{
	std::int64_t value = 0;

	friend bool operator==(const EnumPropertyValue&, const EnumPropertyValue&) = default;
};

// PropertyValue is a variant type that can hold any of the supported property types.
// Static declaration means only the types listed here are supported for properties in the metadata system.
using PropertyValue = std::variant<
	bool,
	std::int64_t,
	std::uint64_t,
	float,
	double,
	std::string,
	Vector2,
	Vector3,
	Vector4,
	Quaternion,
	EnumPropertyValue,
	ActorReference,
	AssetReferenceValue>;

// Represents a single enumration with its declaration name and underlying value.
struct EnumEntry
{
	std::string serializedName;
	std::int64_t value = 0;
};

// Stores the enum as it's declared with the name of the enum type and a list of all the entries in the enum.
class EnumMetadata
{
public:
	std::type_index GetType() const { return m_type; }
	const std::vector<EnumEntry>& GetEntries() const { return m_entries; }

	const EnumEntry* FindByName(std::string_view serializedName) const;
	const EnumEntry* FindByValue(std::int64_t value) const;

private:
	EnumMetadata(std::type_index type, std::vector<EnumEntry> entries)
		: m_type(type), m_entries(std::move(entries))
	{}

	std::type_index m_type;
	std::vector<EnumEntry> m_entries;

	template<class EnumType>
	friend class EnumMetadataBuilder;	// Only built by EnumMetadataBuilder
};

// Unique builder class for constructing EnumMetadata instances.
// Enum entries are declared explicitly once, normally in an EnumReflection specialization.
// Property registration retrieves that declaration from the inferred enum type.
// No compiler-specific enum introspection or source generation is used.
template<class EnumType>
class EnumMetadataBuilder
{
	static_assert(std::is_enum_v<EnumType>, "EnumMetadataBuilder requires an enum type.");
	using Underlying = std::underlying_type_t<EnumType>;
	static_assert(
		std::is_signed_v<Underlying> || sizeof(Underlying) < sizeof(std::int64_t),
		"Unsigned 64-bit enums are not supported.");

public:
	// Adds an entry to the enum metadata.
	EnumMetadataBuilder& Add(std::string serializedName, EnumType value)
	{
		m_entries.push_back({std::move(serializedName), static_cast<std::int64_t>(value)});
		return *this;
	}

	// Builds the EnumMetadata instance. 
	// Returns std::nullopt if the metadata is invalid (e.g., empty or duplicate names and duplicate values).
	std::optional<EnumMetadata> Build() const
	{
		if (m_entries.empty()) return std::nullopt;

		for (std::size_t i = 0; i < m_entries.size(); ++i)
		{
			const EnumEntry& entry = m_entries[i];

			if (entry.serializedName.empty()) return std::nullopt;	// No name is detected

			for (std::size_t j = 0; j < i; ++j)
			{
				if (m_entries[j].serializedName == entry.serializedName ||
					m_entries[j].value == entry.value)
				{
					return std::nullopt;	// Entry dupulication is detected
				}
			}
		}

		return EnumMetadata(std::type_index(typeid(EnumType)), m_entries);
	}

private:
	std::vector<EnumEntry> m_entries;
};

// Specialize once per reflected enum; property declarations infer this metadata from the C++ type.
template<class EnumType>
struct EnumReflection
{
	static std::optional<EnumMetadata> Get() { return std::nullopt; }
};

// Metadata for a property of a type.
class PropertyMetadata
{
public:

	// Type-erased callbacks for accessing a property. TypeMetadataBuilder registers wrappers
	// around callbacks that use the concrete object and value types.
	using ReadCallback = std::function<bool(const void*, PropertyValue&)>;	// Only way to read the property value from the object
	using WriteCallback = std::function<bool(void*, const PropertyValue&)>;	// Only way to write the property value to the object
	
	using ValueValidator = std::function<bool(const PropertyValue&)>;

	const std::string& GetSerializedName() const { return m_serializedName; }
	const PropertyPath& GetPath() const { return m_path; }
	PropertyLogicalType GetLogicalType() const { return m_logicalType; }
	const SerializationMetadata* GetSerializationMetadata() const
	{
		return m_serialization ? &*m_serialization : nullptr;
	}
	const InspectorMetadata* GetInspectorMetadata() const
	{
		return m_inspector ? &*m_inspector : nullptr;
	}
	const EnumMetadata* GetEnumMetadata() const { return m_enumMetadata ? &*m_enumMetadata : nullptr;}
	std::optional<EnumSerializationFormat> GetEnumSerializationFormat() const
	{
		return m_serialization ? m_serialization->enumFormat : std::nullopt;
	}
	AssetType GetAssetType() const { return m_assetType; }

	// Read and Write the property value from/to the given object with registered callbacks. 
	// The object type and property value type must match the registered types in the metadata.
	bool Read(std::type_index objectType, const void* object, PropertyValue& outValue) const;
	bool ValidateValue(const PropertyValue& value) const;
	bool Write(std::type_index objectType, void* object, const PropertyValue& value) const;

private:
	PropertyMetadata(std::string name, PropertyPath path, PropertyLogicalType logicalType,
		std::type_index objectType, std::type_index valueType, AssetType assetType,
		ValueValidator validator, ReadCallback read, WriteCallback write)
		: m_serializedName(std::move(name)), m_path(std::move(path)), m_logicalType(logicalType),
		  m_objectType(objectType), m_valueType(valueType), m_assetType(assetType),
		  m_valueValidator(std::move(validator)), m_read(std::move(read)), m_write(std::move(write))
	{}

	bool IsValid() const;
	bool IsRegisteredEnumValue(const PropertyValue& value) const;

	std::string m_serializedName;
	PropertyPath m_path;
	PropertyLogicalType m_logicalType = PropertyLogicalType::Invalid;

	std::type_index m_objectType;		// The type of the object that owns this property. Read and write callbacks will be invoked on this type ().
	std::type_index m_valueType;		// The type of the property value. Read and write callbacks will be invoked with this type.
	AssetType m_assetType = AssetType::Unknown;
	ValueValidator m_valueValidator;

	// Only way to access the instance's property value from/to the object.
	ReadCallback m_read;
	WriteCallback m_write;

	// Treat enum properties as a special case, since they have a limited set of valid values.
	std::optional<EnumMetadata> m_enumMetadata;
	std::optional<SerializationMetadata> m_serialization = SerializationMetadata{};
	std::optional<InspectorMetadata> m_inspector = InspectorMetadata{};

	template<class ObjectType>
	friend class TypeMetadataBuilder;	// Only built by TypeMetadataBuilder
};

// Metadata for a type, which is constructed from various PropertyMetadata instances.
class TypeMetadata
{
public:
	using ValidationCallback = std::function<std::optional<ReflectionError>(
		std::type_index, const void*)>;

	std::type_index GetType() const { return m_type; }
	const std::string& GetStableTypeName() const { return m_stableTypeName; }
	const std::vector<PropertyMetadata>& GetProperties() const { return m_properties; }
	const PropertyMetadata* FindProperty(std::string_view serializedName) const;
	const PropertyMetadata* FindPropertyByPath(const PropertyPath& path) const;
	bool Validate(
		std::type_index objectType,
		const void* object,
		ReflectionError* outError = nullptr) const;
	bool TryWriteProperty(
		std::type_index objectType,
		void* object,
		const PropertyMetadata& property,
		const PropertyValue& value,
		ReflectionError* outError = nullptr) const;
	bool CopySerializableState(
		std::type_index objectType,
		const void* source,
		void* destination,
		ReflectionError* outError = nullptr) const;

private:
	TypeMetadata(
		std::type_index type,
		std::string stableTypeName,
		std::vector<PropertyMetadata> properties,
		ValidationCallback validator)
		: m_type(type),
		  m_stableTypeName(std::move(stableTypeName)),
		  m_properties(std::move(properties)),
		  m_validator(std::move(validator))
	{}

	std::type_index m_type;			// The type of the object that this metadata represents.
	std::string m_stableTypeName;

	std::vector<PropertyMetadata> m_properties;	// The lists of properties that this type has.
	ValidationCallback m_validator;

	template<class ObjectType>
	friend class TypeMetadataBuilder;	// Only built by TypeMetadataBuilder
};

// Utility to convert concrete types to type erased property values and vice versa.
namespace PropertyMetadataDetail
{
	// CleanType is a pure type without any modifiers (const, volatile, reference).
	template<class ValueType>
	using CleanType = std::remove_cv_t<std::remove_reference_t<ValueType>>;

	// Receive a ValueType and deduce the PropertyLogicalType for it.
	template<class ValueType>
	constexpr PropertyLogicalType DeducedLogicalType()
	{
		using T = CleanType<ValueType>;

		// Explicit the unmatched types T at the time of compilation, 
		// so that avoid compile errors when the unmatched types are used in the code.
		if constexpr (std::is_same_v<T, bool>) return PropertyLogicalType::Bool;
		else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) return PropertyLogicalType::SignedInteger;
		else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) return PropertyLogicalType::UnsignedInteger;
		else if constexpr (std::is_same_v<T, float>) return PropertyLogicalType::Float;
		else if constexpr (std::is_same_v<T, double>) return PropertyLogicalType::Double;
		else if constexpr (std::is_same_v<T, std::string>) return PropertyLogicalType::String;
		else if constexpr (std::is_same_v<T, Vector2>) return PropertyLogicalType::Vector2;
		else if constexpr (std::is_same_v<T, Vector3>) return PropertyLogicalType::Vector3;
		else if constexpr (std::is_same_v<T, Vector4>) return PropertyLogicalType::Vector4;
		else if constexpr (std::is_same_v<T, Quaternion>) return PropertyLogicalType::Quaternion;
		else if constexpr (std::is_enum_v<T>) return PropertyLogicalType::Enum;
		else if constexpr (std::is_same_v<T, ActorReference>) return PropertyLogicalType::ActorReference;
		else if constexpr (IsAssetReferenceV<T>) return PropertyLogicalType::AssetReference;
		else return PropertyLogicalType::Invalid;
	}

	// Receive a ValueType and deduce the AssetType for it.
	template<class ValueType>
	constexpr AssetType DeducedAssetType()
	{
		using T = CleanType<ValueType>;

		if constexpr (IsAssetReferenceV<T>) return T::GetExpectedType();	// Avoid unsupported types T at the time of compilation
		else return AssetType::Unknown;
	}
	
	// Convert a concrete value (given by source) to a PropertyValue. 
	template<class ValueType>
	bool ToPropertyValue(const ValueType& source, PropertyValue& outValue)
	{
		using T = CleanType<ValueType>;

		// Directly supported types are copied into the PropertyValue variant.
		if constexpr (std::is_same_v<T, bool> ||
			std::is_same_v<T, float> ||
			std::is_same_v<T, double> ||
			std::is_same_v<T, std::string> ||
			std::is_same_v<T, Vector2> ||
			std::is_same_v<T, Vector3> ||
			std::is_same_v<T, Vector4> ||
			std::is_same_v<T, Quaternion> ||
			std::is_same_v<T, ActorReference>)
		{
			outValue = source;
			return true;
		}
		else if constexpr (IsAssetReferenceV<T>)
		{
			outValue = source.ToValue();
			return true;
		}

		// Types that are not directly supported by PropertyValue are converted to a compatible type.
		else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
		{// Signed integers to int64_t
			outValue = static_cast<std::int64_t>(source);
			return true;
		}
		else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
		{// Unsigned integers to uint64_t
			outValue = static_cast<std::uint64_t>(source);
			return true;
		}
		else if constexpr (std::is_enum_v<T>)
		{// Enums to EnumPropertyValue (runtime int64_t representation)
			outValue = EnumPropertyValue{ static_cast<std::int64_t>(source) };
			return true;
		}
		else
		{
			return false;
		}
	}
	
	// Convert a PropertyValue to a concrete value (given by outValue). 
	template<class ValueType>
	bool FromPropertyValue(const PropertyValue& source, ValueType& outValue)
	{
		using T = CleanType<ValueType>;

		// Directly supported types are copied from the PropertyValue variant.
		if constexpr (std::is_same_v<T, bool> ||
			std::is_same_v<T, float> ||
			std::is_same_v<T, double> ||
			std::is_same_v<T, std::string> ||
			std::is_same_v<T, Vector2> ||
			std::is_same_v<T, Vector3> ||
			std::is_same_v<T, Vector4> ||
			std::is_same_v<T, Quaternion> ||
			std::is_same_v<T, ActorReference>)
		{
			const T* value = std::get_if<T>(&source); // Conversion from PropertyValue to T, if the type matches.
			if (!value) return false;
			outValue = *value;
			return true;
		}
		else if constexpr (IsAssetReferenceV<T>)
		{
			const AssetReferenceValue* value = std::get_if<AssetReferenceValue>(&source);
			return value && outValue.SetValue(*value);
		}

		// Types that are not directly supported by PropertyValue are converted from a compatible type.
		else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
		{// Signed integers from int64_t
			const std::int64_t* value = std::get_if<std::int64_t>(&source);

			// Check the null and valid range of the int64_t value
			if (!value || *value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
				*value > static_cast<std::int64_t>(std::numeric_limits<T>::max()))
			{
				return false;
			}
			outValue = static_cast<T>(*value);
			return true;
		}
		else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
		{// Unsigned integers from uint64_t
			const std::uint64_t* value = std::get_if<std::uint64_t>(&source);

			// Check the null and valid range of the uint64_t value
			if (!value || *value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) return false;
			outValue = static_cast<T>(*value);
			return true;
		}
		else if constexpr (std::is_enum_v<T>)
		{// Enums from EnumPropertyValue (runtime int64_t representation)
			const EnumPropertyValue* value = std::get_if<EnumPropertyValue>(&source);
			if (!value) return false;
			outValue = static_cast<T>(value->value);
			return true;
		}
		else
		{
			return false;
		}
	}
}

// Constructs metadata from a bounded value type and Component-owned access operations.
template<class ObjectType>
class TypeMetadataBuilder
{
public:
	class ObjectScope;
	explicit TypeMetadataBuilder(std::string stableTypeName) : m_stableTypeName(std::move(stableTypeName)) {}

	template<class Value> using ReadFunction = std::function<bool(const ObjectType&, Value&)>;
	template<class Value> using WriteFunction = std::function<bool(ObjectType&, const Value&)>;
	using Validator = std::function<std::optional<ReflectionError>(const ObjectType&)>;

	template<class Value>
	class PropertyConfiguration
	{
	public:
		PropertyConfiguration(TypeMetadataBuilder& owner, std::size_t index)
			: m_owner(owner), m_index(index < owner.m_properties.size() ? index : std::numeric_limits<std::size_t>::max()) {}

		PropertyConfiguration& Validate(std::function<bool(const Value&)> validator)
		{
			if (auto* p = Get())
				p->m_valueValidator = [validator = std::move(validator)](const PropertyValue& value)
				{
					Value converted{};
					return validator && PropertyMetadataDetail::FromPropertyValue(value, converted) && validator(converted);
				};
			return *this;
		}
		PropertyConfiguration& Serialization(std::optional<SerializationMetadata> facet)
		{
			if (auto* p = Get()) p->m_serialization = std::move(facet);
			return *this;
		}
		PropertyConfiguration& Inspector(std::optional<InspectorMetadata> facet)
		{
			if (auto* p = Get()) p->m_inspector = std::move(facet);
			return *this;
		}
		PropertyConfiguration& Optional()
		{
			if (auto* p = Get(); p && p->m_serialization)
				p->m_serialization->requirement = PropertyRequirement::Optional;
			return *this;
		}
		PropertyConfiguration& SerializedAs(EnumSerializationFormat format)
		{
			if (auto* p = Get(); p && p->m_serialization) p->m_serialization->enumFormat = format;
			return *this;
		}
		// Explicit enum metadata is useful for custom callback registrations and schema validation.
		// Ordinary enum properties use EnumReflection<Value>::Get().
		PropertyConfiguration& Enum(std::optional<EnumMetadata> metadata)
		{
			if (auto* p = Get()) p->m_enumMetadata = std::move(metadata);
			return *this;
		}
	private:
		PropertyMetadata* Get()
		{
			return m_index < m_owner.m_properties.size() ? &m_owner.m_properties[m_index] : nullptr;
		}
		TypeMetadataBuilder& m_owner;
		std::size_t m_index;
	};

	template<class Value>
	auto Property(std::string name, Value ObjectType::* member)
	{
		if (!member) return Accessor<Value>(std::move(name), {}, {});
		return Accessor<Value>(std::move(name),
			[member](const ObjectType& object, Value& value) { value = object.*member; return true; },
			[member](ObjectType& object, const Value& value) { object.*member = value; return true; });
	}

	template<class Getter, class Setter>
	auto Property(std::string name, Getter getter, Setter setter)
	{
		using Value = PropertyMetadataDetail::CleanType<std::invoke_result_t<Getter, const ObjectType&>>;
		if constexpr (std::is_pointer_v<Getter> || std::is_member_pointer_v<Getter>)
			if (!getter) return Accessor<Value>(std::move(name), {}, {});
		if constexpr (std::is_pointer_v<Setter> || std::is_member_pointer_v<Setter>)
			if (!setter) return Accessor<Value>(std::move(name), {}, {});
		return Accessor<Value>(std::move(name),
			[getter](const ObjectType& object, Value& value) { value = std::invoke(getter, object); return true; },
			[setter](ObjectType& object, const Value& value)
			{
				using Result = std::invoke_result_t<Setter, ObjectType&, const Value&>;
				static_assert(std::is_same_v<Result, bool> || std::is_void_v<Result>, "A property setter returns bool or void.");
				if constexpr (std::is_same_v<Result, bool>) return std::invoke(setter, object, value);
				else { std::invoke(setter, object, value); return true; }
			});
	}

	// Select one field in a Component-owned aggregate without recursively reflecting the aggregate.
	template<class Getter, class Setter, class Aggregate, class Value>
	auto Property(std::string name, Getter getter, Setter setter, Value Aggregate::* member)
	{
		if (!member) return Accessor<Value>(std::move(name), {}, {});
		if constexpr (std::is_pointer_v<Getter> || std::is_member_pointer_v<Getter>)
			if (!getter) return Accessor<Value>(std::move(name), {}, {});
		if constexpr (std::is_pointer_v<Setter> || std::is_member_pointer_v<Setter>)
			if (!setter) return Accessor<Value>(std::move(name), {}, {});
		return Property(std::move(name),
			[getter, member](const ObjectType& object) { return std::invoke(getter, object).*member; },
			[getter, setter, member](ObjectType& object, const Value& value)
			{
				auto aggregate = std::invoke(getter, object);
				aggregate.*member = value;
				return std::invoke(setter, object, aggregate);
			});
	}

	// Fallible reads and exceptional access operations use the same property/facet representation.
	template<class Value>
	auto Accessor(std::string name, ReadFunction<Value> read, WriteFunction<Value> write)
	{
		const auto index = m_properties.size();
		AddAccessorAtPath<Value>({std::move(name)}, std::move(read), std::move(write));
		return PropertyConfiguration<Value>(*this, index);
	}

	TypeMetadataBuilder& SetValidator(Validator validator)
	{
		m_validator = std::move(validator);
		return *this;
	}
	template<class Callback>
	TypeMetadataBuilder& Object(std::string name, Callback callback)
	{
		AddObject({std::move(name)}, std::move(callback));
		return *this;
	}

	// Build the TypeMetadata instance.
	std::optional<TypeMetadata> Build() const
	{
		if (m_stableTypeName.empty() || !m_registrationValid) return std::nullopt;

		for (std::size_t i = 0; i < m_properties.size(); ++i)
		{
			const PropertyMetadata& property = m_properties[i];
			if (!property.IsValid()) return std::nullopt;

			for (std::size_t j = 0; j < i; ++j)
			{
				const PropertyPath& other = m_properties[j].GetPath();
				if (other == property.GetPath() ||
					IsAncestor(other, property.GetPath()) ||
					IsAncestor(property.GetPath(), other)) return std::nullopt;
			}

			for (const PropertyPath& objectPath : m_objectPaths)
			{
				if (objectPath == property.GetPath()) return std::nullopt;
			}
		}

		for (const PropertyPath& objectPath : m_objectPaths)
		{
			bool hasLeaf = false;
			for (const PropertyMetadata& property : m_properties)
			{
				if (IsAncestor(objectPath, property.GetPath()))
				{
					hasLeaf = true;
					break;
				}
			}
			if (!hasLeaf) return std::nullopt;
		}

		return TypeMetadata(
			std::type_index(typeid(ObjectType)),
			m_stableTypeName,
			m_properties,
			[m_validator = m_validator](std::type_index objectType, const void* object)
				-> std::optional<ReflectionError>
			{
				if (!m_validator) return std::nullopt;
				if (!object || objectType != std::type_index(typeid(ObjectType)))
				{
					return ReflectionError{
						ReflectionErrorCode::InvalidObject,
						std::nullopt,
						"Object type does not match its reflection metadata." };
				}
				return m_validator(*static_cast<const ObjectType*>(object));
			});
	}

	class ObjectScope
	{
	public:
		template<class... Access>
		auto Property(std::string name, Access... access)
		{
			const auto index = m_owner.m_properties.size();
			auto configuration = m_owner.Property(name, access...);
			Relocate(index, std::move(name));
			return configuration;
		}
		template<class Value>
		auto Accessor(std::string name, ReadFunction<Value> read, WriteFunction<Value> write)
		{
			const auto index = m_owner.m_properties.size();
			auto configuration = m_owner.template Accessor<Value>(name, std::move(read), std::move(write));
			Relocate(index, std::move(name));
			return configuration;
		}
		template<class Callback>
		ObjectScope& Object(std::string name, Callback callback)
		{
			m_owner.AddObject(ChildPath(std::move(name)), std::move(callback));
			return *this;
		}
	private:
		ObjectScope(TypeMetadataBuilder& owner, std::vector<std::string> path)
			: m_owner(owner), m_path(std::move(path)) {}
		std::vector<std::string> ChildPath(std::string name) const
		{
			auto path = m_path;
			path.push_back(std::move(name));
			return path;
		}
		void Relocate(std::size_t index, std::string name)
		{
			const auto path = PropertyPath::FromMembers(ChildPath(std::move(name)));
			if (!path) m_owner.m_registrationValid = false;
			else if (index < m_owner.m_properties.size()) m_owner.m_properties[index].m_path = *path;
		}
		TypeMetadataBuilder& m_owner;
		std::vector<std::string> m_path;
		friend class TypeMetadataBuilder;
	};

private:
	template<class Value>
	void AddAccessorAtPath(std::vector<std::string> members, ReadFunction<Value> read, WriteFunction<Value> write)
	{
		const auto path = PropertyPath::FromMembers(members);
		if (!path) { m_registrationValid = false; return; }
		PropertyMetadata::ReadCallback erasedRead;
		PropertyMetadata::WriteCallback erasedWrite;
		if (read)
			erasedRead = [read = std::move(read)](const void* object, PropertyValue& out)
			{
				Value value{};
				return read(*static_cast<const ObjectType*>(object), value) && PropertyMetadataDetail::ToPropertyValue(value, out);
			};
		if (write)
			erasedWrite = [write = std::move(write)](void* object, const PropertyValue& value)
			{
				Value converted{};
				return PropertyMetadataDetail::FromPropertyValue(value, converted) && write(*static_cast<ObjectType*>(object), converted);
			};
		PropertyMetadata property(members.back(), *path, PropertyMetadataDetail::DeducedLogicalType<Value>(),
			typeid(ObjectType), typeid(Value), PropertyMetadataDetail::DeducedAssetType<Value>(),
			[](const PropertyValue& value) { Value converted{}; return PropertyMetadataDetail::FromPropertyValue(value, converted); },
			std::move(erasedRead), std::move(erasedWrite));
		if constexpr (std::is_enum_v<Value>)
		{
			property.m_enumMetadata = EnumReflection<Value>::Get();
			property.m_serialization->enumFormat = EnumSerializationFormat::Name;
		}
		m_properties.push_back(std::move(property));
	}

	template<class Callback>
	void AddObject(std::vector<std::string> pathMembers, Callback callback)
	{
		const auto path = PropertyPath::FromMembers(pathMembers);
		if (!path)
		{
			m_registrationValid = false;
			return;
		}
		for (const PropertyPath& existing : m_objectPaths)
		{
			if (existing == *path)
			{
				m_registrationValid = false;
				return;
			}
		}

		m_objectPaths.push_back(*path);
		ObjectScope scope(*this, std::move(pathMembers));
		callback(scope);
	}

	static bool IsAncestor(const PropertyPath& ancestor, const PropertyPath& descendant)
	{
		const auto& left = ancestor.GetMembers();
		const auto& right = descendant.GetMembers();
		return left.size() < right.size() &&
			std::equal(left.begin(), left.end(), right.begin());
	}

	std::string m_stableTypeName;
	std::vector<PropertyMetadata> m_properties;
	std::vector<PropertyPath> m_objectPaths;
	Validator m_validator;
	bool m_registrationValid = true;
};
