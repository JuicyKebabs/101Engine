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
	Color,
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

struct InspectorMetadata
{
	std::string label;
	bool readOnly = false;
	std::optional<NumericEditorMetadata> numeric;
};

struct SerializationMetadata
{
	PropertyRequirement requirement = PropertyRequirement::Required;
	std::optional<EnumSerializationFormat> enumFormat;
};

// Compatibility flags accepted by existing builder calls. PropertyMetadata converts these
// flags into independent serialization and Inspector facets when it is constructed.
enum class PropertyPolicy : std::uint8_t
{
	None = 0,
	Serializable = 1u << 0,
	Inspectable = 1u << 1,
	EditorReadOnly = 1u << 2,
};

constexpr PropertyPolicy operator|(PropertyPolicy lhs, PropertyPolicy rhs)
{
	return static_cast<PropertyPolicy>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr bool HasPropertyPolicy(PropertyPolicy policies, PropertyPolicy policy)
{
	return (static_cast<std::uint8_t>(policies) & static_cast<std::uint8_t>(policy)) != 0;
}

constexpr PropertyPolicy DefaultPropertyPolicy()
{
	return PropertyPolicy::Serializable | PropertyPolicy::Inspectable;
}

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
// This has no function to analyze the enum type at compile time and register its entries. 
// Instead, the user must call Add() for each entry in the enum (Has to be improved....)
// Receive the enum type as a template parameter and provide a interface to add entries and build the metadata for that enum type.
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
	PropertyPolicy GetPolicy() const { return m_policy; }
	PropertyRequirement GetRequirement() const { return m_requirement; }
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
		return m_enumSerializationFormat;
	}
	AssetType GetAssetType() const { return m_assetType; }

	// Read and Write the property value from/to the given object with registered callbacks. 
	// The object type and property value type must match the registered types in the metadata.
	bool Read(std::type_index objectType, const void* object, PropertyValue& outValue) const;
	bool ValidateValue(const PropertyValue& value) const;
	bool Write(std::type_index objectType, void* object, const PropertyValue& value) const;

private:
	PropertyMetadata(
		std::string serializedName,
		PropertyPath path,
		PropertyLogicalType logicalType,
		PropertyPolicy policy,
		PropertyRequirement requirement,
		std::type_index objectType,
		std::type_index valueType,
		bool valueTypeCompatible,
		AssetType assetType,
		ValueValidator valueValidator,
		ReadCallback read,
		WriteCallback write,
		std::optional<EnumMetadata> enumMetadata,
		std::optional<EnumSerializationFormat> enumSerializationFormat,
		std::optional<InspectorMetadata> inspectorMetadata)
		: m_serializedName(std::move(serializedName)),
		  m_path(std::move(path)),
		  m_logicalType(logicalType),
		  m_policy(policy),
		  m_requirement(requirement),
		  m_objectType(objectType),
		  m_valueType(valueType),
		  m_valueTypeCompatible(valueTypeCompatible),
		  m_assetType(assetType),
		  m_valueValidator(std::move(valueValidator)),
		  m_read(std::move(read)),
		  m_write(std::move(write)),
		  m_enumMetadata(std::move(enumMetadata)),
		  m_enumSerializationFormat(enumSerializationFormat)
	{
		if (HasPropertyPolicy(policy, PropertyPolicy::Serializable))
		{
			m_serialization = SerializationMetadata{ requirement, enumSerializationFormat };
		}

		if (HasPropertyPolicy(policy, PropertyPolicy::Inspectable))
		{
			if (inspectorMetadata)
			{
				m_inspector = std::move(*inspectorMetadata);
			}
			else
			{
				m_inspector = InspectorMetadata{};
			}
			m_inspector->readOnly = m_inspector->readOnly ||
				HasPropertyPolicy(policy, PropertyPolicy::EditorReadOnly);
		}
	}

	bool IsValid() const;
	bool IsRegisteredEnumValue(const PropertyValue& value) const;

	std::string m_serializedName;
	PropertyPath m_path;
	PropertyLogicalType m_logicalType = PropertyLogicalType::Invalid;
	PropertyPolicy m_policy = PropertyPolicy::None;
	PropertyRequirement m_requirement = PropertyRequirement::Required;

	std::type_index m_objectType;		// The type of the object that owns this property. Read and write callbacks will be invoked on this type ().
	std::type_index m_valueType;		// The type of the property value. Read and write callbacks will be invoked with this type.
	bool m_valueTypeCompatible = false;	// Whether the value type is compatible with the logical type. This is used to validate the property metadata.
	AssetType m_assetType = AssetType::Unknown;
	ValueValidator m_valueValidator;

	// Only way to access the instance's property value from/to the object.
	ReadCallback m_read;
	WriteCallback m_write;

	// Treat enum properties as a special case, since they have a limited set of valid values.
	std::optional<EnumMetadata> m_enumMetadata;
	std::optional<EnumSerializationFormat> m_enumSerializationFormat;
	std::optional<SerializationMetadata> m_serialization;
	std::optional<InspectorMetadata> m_inspector;

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
	
	// Check if a given logical type is compatible with a ValueType.
	template<class ValueType>
	bool IsLogicalTypeCompatible(PropertyLogicalType logicalType)
	{
		using T = CleanType<ValueType>;
		const PropertyLogicalType deduced = DeducedLogicalType<T>();

		// Special case: Vector4 and Color are considered compatible, since they are both represented as 4 floats.
		return logicalType == deduced || (std::is_same_v<T, Vector4> && logicalType == PropertyLogicalType::Color);
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

// Unique builder class for constructing TypeMetadata instances.
// Instanciate this for a specific ObjectType and use AddMember() or AddAccessorProperty() to add properties, then call Build() to get the TypeMetadata.
template<class ObjectType>
class TypeMetadataBuilder
{
public:
	class ObjectScope;

	explicit TypeMetadataBuilder(std::string stableTypeName)
		: m_stableTypeName(std::move(stableTypeName))
	{}

	// Aliases for the read and write callbacks that operate on the ObjectType and a specific ValueType.
	template<class ValueType>
	using ReadFunction = std::function<bool(const ObjectType&, ValueType&)>;

	template<class ValueType>
	using WriteFunction = std::function<bool(ObjectType&, const ValueType&)>;
	
	// Adds callbacks for a specific property, allowing custom read and write behavior.
	// Reflected property is decided in the read and write callbacks,
	template<class ValueType>
	TypeMetadataBuilder& AddAccessorProperty(
		std::string serializedName,
		PropertyLogicalType logicalType,
		PropertyPolicy policy,
		ReadFunction<ValueType> read,
		WriteFunction<ValueType> write,
		std::optional<EnumMetadata> enumMetadata = std::nullopt,
		std::optional<EnumSerializationFormat> enumSerializationFormat = std::nullopt,
		PropertyRequirement requirement = PropertyRequirement::Required,
		std::optional<InspectorMetadata> inspectorMetadata = std::nullopt)
	{
		AddAccessorAtPath<ValueType>(
			{ serializedName }, logicalType, policy,
			std::move(read), std::move(write),
			std::move(enumMetadata), enumSerializationFormat, requirement,
			std::move(inspectorMetadata));
		return *this;
	}

	template<class ValueType>
	TypeMetadataBuilder& AddAccessorProperty(
		std::string serializedName,
		ReadFunction<ValueType> read,
		WriteFunction<ValueType> write,
		PropertyPolicy policy = DefaultPropertyPolicy(),
		PropertyRequirement requirement = PropertyRequirement::Required,
		std::optional<InspectorMetadata> inspectorMetadata = std::nullopt)
	{
		return AddAccessorProperty<ValueType>(
			std::move(serializedName),
			PropertyMetadataDetail::DeducedLogicalType<ValueType>(),
			policy,
			std::move(read),
			std::move(write),
			std::nullopt,
			std::nullopt,
			requirement,
			std::move(inspectorMetadata));
	}

	using Validator = std::function<std::optional<ReflectionError>(const ObjectType&)>;

	TypeMetadataBuilder& SetValidator(Validator validator)
	{
		m_validator = std::move(validator);
		return *this;
	}

	// Add a member directly from the ObjectType without needing to write custom read and write callbacks.
	// Do not subscribe the member by this if the member has to be read or written in a special way 
	// (e.g., computed properties, change of this property has to affect other properties).
	template<class ValueType>
	TypeMetadataBuilder& AddMember(
		std::string serializedName,
		ValueType ObjectType::* member,
		PropertyPolicy policy = DefaultPropertyPolicy(),
		PropertyLogicalType logicalType = PropertyMetadataDetail::DeducedLogicalType<ValueType>(),
		PropertyRequirement requirement = PropertyRequirement::Required)
	{
		AddMemberAtPath<ValueType>({ serializedName }, member, policy, logicalType, requirement);
		return *this;
	}

	template<class EnumType>
	TypeMetadataBuilder& AddEnumMember(
		std::string serializedName,
		EnumType ObjectType::* member,
		const EnumMetadata& enumMetadata,
		PropertyPolicy policy = DefaultPropertyPolicy(),
		EnumSerializationFormat format = EnumSerializationFormat::Name,
		PropertyRequirement requirement = PropertyRequirement::Required)
	{
		AddEnumMemberAtPath(
			{ serializedName }, member, enumMetadata, policy, format, requirement);
		return *this;
	}

	template<class Callback>
	TypeMetadataBuilder& Object(std::string objectName, Callback callback)
	{
		std::vector<std::string> path{ std::move(objectName) };
		AddObject(path, std::move(callback));
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
		template<class ValueType>
		ObjectScope& AddAccessor(
			std::string serializedName,
			PropertyLogicalType logicalType,
			PropertyPolicy policy,
			ReadFunction<ValueType> read,
			WriteFunction<ValueType> write,
			std::optional<EnumMetadata> enumMetadata = std::nullopt,
			std::optional<EnumSerializationFormat> enumSerializationFormat = std::nullopt,
			PropertyRequirement requirement = PropertyRequirement::Required,
			std::optional<InspectorMetadata> inspectorMetadata = std::nullopt)
		{
			auto path = ChildPath(std::move(serializedName));
			m_owner.template AddAccessorAtPath<ValueType>(
				std::move(path), logicalType, policy,
				std::move(read), std::move(write),
				std::move(enumMetadata), enumSerializationFormat, requirement,
				std::move(inspectorMetadata));
			return *this;
		}

		template<class ValueType>
		ObjectScope& AddAccessor(
			std::string serializedName,
			ReadFunction<ValueType> read,
			WriteFunction<ValueType> write,
			PropertyPolicy policy = DefaultPropertyPolicy(),
			PropertyRequirement requirement = PropertyRequirement::Required,
			std::optional<InspectorMetadata> inspectorMetadata = std::nullopt)
		{
			return AddAccessor<ValueType>(
				std::move(serializedName),
				PropertyMetadataDetail::DeducedLogicalType<ValueType>(),
				policy,
				std::move(read),
				std::move(write),
				std::nullopt,
				std::nullopt,
				requirement,
				std::move(inspectorMetadata));
		}

		template<class ValueType>
		ObjectScope& AddMember(
			std::string serializedName,
			ValueType ObjectType::* member,
			PropertyPolicy policy = DefaultPropertyPolicy(),
			PropertyLogicalType logicalType = PropertyMetadataDetail::DeducedLogicalType<ValueType>(),
			PropertyRequirement requirement = PropertyRequirement::Required)
		{
			m_owner.template AddMemberAtPath<ValueType>(
				ChildPath(std::move(serializedName)), member, policy, logicalType, requirement);
			return *this;
		}

		template<class EnumType>
		ObjectScope& AddEnumMember(
			std::string serializedName,
			EnumType ObjectType::* member,
			const EnumMetadata& enumMetadata,
			PropertyPolicy policy = DefaultPropertyPolicy(),
			EnumSerializationFormat format = EnumSerializationFormat::Name,
			PropertyRequirement requirement = PropertyRequirement::Required)
		{
			m_owner.AddEnumMemberAtPath(
				ChildPath(std::move(serializedName)), member,
				enumMetadata, policy, format, requirement);
			return *this;
		}

		template<class Callback>
		ObjectScope& Object(std::string objectName, Callback callback)
		{
			m_owner.AddObject(ChildPath(std::move(objectName)), std::move(callback));
			return *this;
		}

	private:
		ObjectScope(TypeMetadataBuilder& owner, std::vector<std::string> path)
			: m_owner(owner), m_path(std::move(path))
		{}

		std::vector<std::string> ChildPath(std::string name) const
		{
			auto path = m_path;
			path.push_back(std::move(name));
			return path;
		}

		TypeMetadataBuilder& m_owner;
		std::vector<std::string> m_path;

		friend class TypeMetadataBuilder;
	};

private:
	template<class ValueType>
	void AddAccessorAtPath(
		std::vector<std::string> pathMembers,
		PropertyLogicalType logicalType,
		PropertyPolicy policy,
		ReadFunction<ValueType> read,
		WriteFunction<ValueType> write,
		std::optional<EnumMetadata> enumMetadata,
		std::optional<EnumSerializationFormat> enumSerializationFormat,
		PropertyRequirement requirement,
		std::optional<InspectorMetadata> inspectorMetadata)
	{
		using T = PropertyMetadataDetail::CleanType<ValueType>;
		const auto path = PropertyPath::FromMembers(pathMembers);
		if (!path)
		{
			m_registrationValid = false;
			return;
		}

		PropertyMetadata::ValueValidator valueValidator = [](const PropertyValue& value)
		{
			T converted{};
			return PropertyMetadataDetail::FromPropertyValue(value, converted);
		};

		PropertyMetadata::ReadCallback erasedRead;
		PropertyMetadata::WriteCallback erasedWrite;
		if (read)
		{
			erasedRead = [read = std::move(read)](const void* object, PropertyValue& outValue)
			{
				T value{};
				if (!read(*static_cast<const ObjectType*>(object), value)) return false;
				return PropertyMetadataDetail::ToPropertyValue(value, outValue);
			};
		}
		if (write)
		{
			erasedWrite = [write = std::move(write)](void* object, const PropertyValue& value)
			{
				T converted{};
				if (!PropertyMetadataDetail::FromPropertyValue(value, converted)) return false;
				return write(*static_cast<ObjectType*>(object), converted);
			};
		}

		m_properties.push_back(PropertyMetadata(
			pathMembers.back(), *path, logicalType, policy, requirement,
			std::type_index(typeid(ObjectType)), std::type_index(typeid(T)),
			PropertyMetadataDetail::IsLogicalTypeCompatible<T>(logicalType),
			PropertyMetadataDetail::DeducedAssetType<T>(),
			std::move(valueValidator), std::move(erasedRead), std::move(erasedWrite),
			std::move(enumMetadata), enumSerializationFormat,
			std::move(inspectorMetadata)));
	}

	template<class ValueType>
	void AddMemberAtPath(
		std::vector<std::string> pathMembers,
		ValueType ObjectType::* member,
		PropertyPolicy policy,
		PropertyLogicalType logicalType,
		PropertyRequirement requirement)
	{
		if (member == nullptr)
		{
			AddAccessorAtPath<ValueType>(
				std::move(pathMembers), logicalType, policy, {}, {}, {}, {},
				requirement, std::nullopt);
			return;
		}

		AddAccessorAtPath<ValueType>(
			std::move(pathMembers), logicalType, policy,
			[member](const ObjectType& object, ValueType& outValue)
			{
				outValue = object.*member;
				return true;
			},
			[member](ObjectType& object, const ValueType& value)
			{
				object.*member = value;
				return true;
			}, {}, {}, requirement, std::nullopt);
	}

	template<class EnumType>
	void AddEnumMemberAtPath(
		std::vector<std::string> pathMembers,
		EnumType ObjectType::* member,
		const EnumMetadata& enumMetadata,
		PropertyPolicy policy,
		EnumSerializationFormat format,
		PropertyRequirement requirement)
	{
		static_assert(std::is_enum_v<EnumType>, "AddEnumMember requires an enum member.");
		AddAccessorAtPath<EnumType>(
			std::move(pathMembers), PropertyLogicalType::Enum, policy,
			[member](const ObjectType& object, EnumType& outValue)
			{
				outValue = object.*member;
				return true;
			},
			[member](ObjectType& object, const EnumType& value)
			{
				object.*member = value;
				return true;
			}, enumMetadata, format, requirement, std::nullopt);
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
