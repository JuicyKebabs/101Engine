#include "ReflectionSerialization.h"
#include "ActorReferenceCodec.h"
#include "AssetReferenceCodec.h"
#include "PropertyMetadata.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <map>
#include <utility>
#include <vector>

// Conversion helper functions for serializing and deserializing reflection data to and from JSON.
// The property values are converted between JSON and type-erased property values based on their logical types.
namespace
{
	using json = nlohmann::json;

	struct PropertySchemaNode
	{
		const PropertyMetadata* property = nullptr;
		std::map<std::string, PropertySchemaNode> children;
	};

	bool BuildSchema(
		const std::vector<const PropertyMetadata*>& properties,
		PropertySchemaNode& root)
	{
		for (const PropertyMetadata* property : properties)
		{
			PropertySchemaNode* node = &root;
			for (const std::string& member : property->GetPath().GetMembers())
			{
				if (node->property) return false;
				node = &node->children[member];
			}
			if (node->property || !node->children.empty()) return false;
			node->property = property;
		}
		return true;
	}

	bool ValidateSchema(const json& source, const PropertySchemaNode& schema)
	{
		if (!source.is_object()) return false;

		for (auto member = source.begin(); member != source.end(); ++member)
		{
			if (!schema.children.contains(member.key())) return false;
		}

		for (const auto& [name, childSchema] : schema.children)
		{
			const auto child = source.find(name);
			if (child == source.end())
			{
				if (childSchema.property &&
					childSchema.property->GetRequirement() == PropertyRequirement::Optional)
				{
					continue;
				}
				return false;
			}
			if (childSchema.property)
			{
				if (!childSchema.children.empty()) return false;
			}
			else if (!ValidateSchema(*child, childSchema))
			{
				return false;
			}
		}
		return true;
	}

	bool SetValueAtPath(json& root, const PropertyPath& path, json value)
	{
		json* object = &root;
		const auto& members = path.GetMembers();
		for (std::size_t i = 0; i + 1 < members.size(); ++i)
		{
			json& child = (*object)[members[i]];
			if (child.is_null()) child = json::object();
			if (!child.is_object()) return false;
			object = &child;
		}

		if (object->contains(members.back())) return false;
		(*object)[members.back()] = std::move(value);
		return true;
	}

	const json* FindValueAtPath(const json& root, const PropertyPath& path)
	{
		const json* value = &root;
		for (const std::string& member : path.GetMembers())
		{
			if (!value->is_object()) return nullptr;
			const auto child = value->find(member);
			if (child == value->end()) return nullptr;
			value = &*child;
		}
		return value;
	}

	bool IsFinite(float value)
	{
		return std::isfinite(value);
	}

	bool IsFinite(double value)
	{
		return std::isfinite(value);
	}

	bool TryReadFloat(const json& source, float& outValue)
	{
		if (!source.is_number_float()) return false;

		const double value = source.get<double>();
		const float converted = static_cast<float>(value);
		if (!IsFinite(value) || !IsFinite(converted)) return false;

		outValue = converted;
		return true;
	}

	bool SerializeFloatArray(std::initializer_list<float> values, json& outJson)
	{
		for (float value : values)
		{
			if (!IsFinite(value)) return false;
		}

		outJson = json::array();
		for (float value : values)
		{
			outJson.push_back(value);
		}
		return true;
	}

	template<std::size_t Size>
	bool DeserializeFloatArray(const json& source, std::array<float, Size>& outValues)
	{
		if (!source.is_array() || source.size() != Size) return false;

		for (std::size_t i = 0; i < Size; ++i)
		{
			if (!TryReadFloat(source[i], outValues[i])) return false;
		}
		return true;
	}

	bool SerializePropertyValue(
		const PropertyMetadata& property,
		const PropertyValue& value,
		json& outJson,
		ReflectionSaveContext context)
	{
		switch (property.GetLogicalType())
		{
		case PropertyLogicalType::Bool:
			if (const bool* typed = std::get_if<bool>(&value))
			{
				outJson = *typed;
				return true;
			}
			return false;

		case PropertyLogicalType::SignedInteger:
			if (const std::int64_t* typed = std::get_if<std::int64_t>(&value))
			{
				outJson = *typed;
				return true;
			}
			return false;

		case PropertyLogicalType::UnsignedInteger:
			if (const std::uint64_t* typed = std::get_if<std::uint64_t>(&value))
			{
				outJson = *typed;
				return true;
			}
			return false;

		case PropertyLogicalType::Float:
			if (const float* typed = std::get_if<float>(&value); typed && IsFinite(*typed))
			{
				outJson = *typed;
				return true;
			}
			return false;

		case PropertyLogicalType::Double:
			if (const double* typed = std::get_if<double>(&value); typed && IsFinite(*typed))
			{
				outJson = *typed;
				return true;
			}
			return false;

		case PropertyLogicalType::String:
			if (const std::string* typed = std::get_if<std::string>(&value))
			{
				outJson = *typed;
				return true;
			}
			return false;

		case PropertyLogicalType::Vector2:
			if (const Vector2* typed = std::get_if<Vector2>(&value))
				return SerializeFloatArray({ typed->x, typed->y }, outJson);
			return false;

		case PropertyLogicalType::Vector3:
			if (const Vector3* typed = std::get_if<Vector3>(&value))
				return SerializeFloatArray({ typed->x, typed->y, typed->z }, outJson);
			return false;

		case PropertyLogicalType::Vector4:
		case PropertyLogicalType::Color:
			if (const Vector4* typed = std::get_if<Vector4>(&value))
				return SerializeFloatArray({ typed->x, typed->y, typed->z, typed->w }, outJson);
			return false;

		case PropertyLogicalType::Quaternion:
			if (const Quaternion* typed = std::get_if<Quaternion>(&value))
				return SerializeFloatArray({ typed->x, typed->y, typed->z, typed->w }, outJson);
			return false;

		case PropertyLogicalType::Enum:
		{
			const EnumPropertyValue* typed = std::get_if<EnumPropertyValue>(&value);
			const EnumMetadata* enumMetadata = property.GetEnumMetadata();
			const EnumEntry* entry = nullptr;
			if (typed && enumMetadata)
			{
				entry = enumMetadata->FindByValue(typed->value);
			}
			if (!entry) return false;

			const auto format = property.GetEnumSerializationFormat();
			if (!format) return false;
			if (*format == EnumSerializationFormat::Name)
			{
				outJson = entry->serializedName;
			}
			else
			{
				outJson = entry->value;
			}
			return true;
		}

		case PropertyLogicalType::ActorReference:
		{
			const ActorReference* typed = std::get_if<ActorReference>(&value);

			if (!typed || !context.actorReferenceCodec || !context.actorReferenceContext)
			{
				return false;
			}

			// Serialize the ActorReference with the provided ActorReferenceCodec and context.
			ActorReferenceCodecResult result;
			result = context.actorReferenceCodec->Serialize(*typed, *context.actorReferenceContext, outJson);

			if (result != ActorReferenceCodecResult::Success)
			{
				return false;
			}

			return true;
		}

		case PropertyLogicalType::AssetReference:
		{
			const AssetReferenceValue* typed = std::get_if<AssetReferenceValue>(&value);

			if (!typed || !context.assetReferenceCodec || !context.assetReferenceContext)
			{
				return false;
			}

			// Serialize the AssetReference with the provided AssetReferenceCodec and context.
			AssetReferenceCodecResult result;
			result = context.assetReferenceCodec->Serialize(*typed, *context.assetReferenceContext, outJson);

			if (result != AssetReferenceCodecResult::Success)
			{
				return false;
			}

			return true;
		}

		default:
			return false;
		}
	}

	bool DeserializePropertyValue(
		const PropertyMetadata& property,
		const json& source,
		PropertyValue& outValue,
		ReflectionRestoreContext context)
	{
		switch (property.GetLogicalType())
		{
		case PropertyLogicalType::Bool:
			if (!source.is_boolean()) return false;
			outValue = source.get<bool>();
			return true;

		case PropertyLogicalType::SignedInteger:
			if (!source.is_number_integer()) return false;
			if (source.is_number_unsigned())
			{
				const std::uint64_t value = source.get<std::uint64_t>();
				if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
				outValue = static_cast<std::int64_t>(value);
			}
			else
			{
				outValue = source.get<std::int64_t>();
			}
			return true;

		case PropertyLogicalType::UnsignedInteger:
			if (!source.is_number_integer()) return false;
			if (source.is_number_unsigned())
			{
				outValue = source.get<std::uint64_t>();
			}
			else
			{
				const std::int64_t value = source.get<std::int64_t>();
				if (value < 0) return false;
				outValue = static_cast<std::uint64_t>(value);
			}
			return true;

		case PropertyLogicalType::Float:
		{
			float value = 0.0f;
			if (!TryReadFloat(source, value)) return false;
			outValue = value;
			return true;
		}

		case PropertyLogicalType::Double:
			if (!source.is_number_float()) return false;
			if (const double value = source.get<double>(); IsFinite(value))
			{
				outValue = value;
				return true;
			}
			return false;

		case PropertyLogicalType::String:
			if (!source.is_string()) return false;
			outValue = source.get<std::string>();
			return true;

		case PropertyLogicalType::Vector2:
		{
			std::array<float, 2> values;
			if (!DeserializeFloatArray(source, values)) return false;
			outValue = Vector2(values.data());
			return true;
		}

		case PropertyLogicalType::Vector3:
		{
			std::array<float, 3> values;
			if (!DeserializeFloatArray(source, values)) return false;
			outValue = Vector3(values.data());
			return true;
		}

		case PropertyLogicalType::Vector4:
		case PropertyLogicalType::Color:
		{
			std::array<float, 4> values;
			if (!DeserializeFloatArray(source, values)) return false;
			outValue = Vector4(values.data());
			return true;
		}

		case PropertyLogicalType::Quaternion:
		{
			std::array<float, 4> values;
			if (!DeserializeFloatArray(source, values)) return false;
			outValue = Quaternion(values.data());
			return true;
		}

		case PropertyLogicalType::Enum:
		{
			const EnumMetadata* enumMetadata = property.GetEnumMetadata();
			const auto format = property.GetEnumSerializationFormat();
			if (!enumMetadata || !format) return false;

			const EnumEntry* entry = nullptr;
			if (*format == EnumSerializationFormat::Name)
			{
				if (!source.is_string()) return false;
				entry = enumMetadata->FindByName(source.get<std::string>());
			}
			else
			{
				if (!source.is_number_integer()) return false;
				std::int64_t value = 0;
				if (source.is_number_unsigned())
				{
					const std::uint64_t unsignedValue = source.get<std::uint64_t>();
					if (unsignedValue > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
						return false;
					value = static_cast<std::int64_t>(unsignedValue);
				}
				else
				{
					value = source.get<std::int64_t>();
				}
				entry = enumMetadata->FindByValue(value);
			}
			if (!entry) return false;

			outValue = EnumPropertyValue{ entry->value };
			return true;
		}

		case PropertyLogicalType::ActorReference:
		{
			if (!context.actorReferenceCodec) return false;

			ActorReference reference;
			ActorReferenceCodecResult result;

			// Deserialize the ActorReference with the provided ActorReferenceCodec.
			result = context.actorReferenceCodec->Deserialize(source, reference);

			if (result != ActorReferenceCodecResult::Success)
			{
				return false;
			}

			outValue = reference;
			return true;
		}

		case PropertyLogicalType::AssetReference:
		{
			if (!context.assetReferenceCodec) return false;

			AssetReferenceValue reference;
			AssetReferenceCodecResult result;

			// Deserialize the AssetReference with the provided AssetReferenceCodec.
			result = context.assetReferenceCodec->Deserialize(source, property.GetAssetType(), reference);

			if (result != AssetReferenceCodecResult::Success)
			{
				return false;
			}

			outValue = reference;
			return true;
		}

		default:
			return false;
		}
	}

	std::vector<const PropertyMetadata*> GetSerializableProperties(const TypeMetadata& metadata)
	{
		std::vector<const PropertyMetadata*> properties;

		// Collect properties that provide a serialization facet.
		for (const PropertyMetadata& property : metadata.GetProperties())
		{
			if (property.GetSerializationMetadata())
			{
				properties.push_back(&property);
			}
		}

		// Sort the properties by their serialized names to ensure consistent ordering in the JSON output.
		std::sort(properties.begin(), properties.end(), [](const auto* lhs, const auto* rhs)
		{
			return lhs->GetPath().ToString() < rhs->GetPath().ToString();
		});

		return properties;
	}

	void SetError(
		ReflectionError* outError,
		ReflectionErrorCode code,
		std::optional<PropertyPath> path,
		std::string message)
	{
		if (!outError) return;
		*outError = { code, std::move(path), std::move(message) };
	}
}

bool ReflectionSerializer::Serialize(
	const TypeMetadata& metadata,
	std::type_index objectType,
	const void* object,
	nlohmann::json& outJson,
	ReflectionSaveContext context,
	ReflectionError* outError)
{
	if (!object || objectType != metadata.GetType())
	{
		SetError(outError, ReflectionErrorCode::InvalidObject, std::nullopt,
			"Object type does not match its reflection metadata.");
		return false;
	}

	json result = json::object();

	// Iterate over the serializable properties and serialize each one into the JSON object.
	for (const PropertyMetadata* property : GetSerializableProperties(metadata))
	{
		PropertyValue value;
		json serializedValue;

		// Read the property value from the object and serialize it into JSON.
		if (!property->Read(objectType, object, value))
		{
			SetError(outError, ReflectionErrorCode::PropertyReadFailed,
				property->GetPath(), "Failed to read a serializable property.");
			return false;
		}

		if (!SerializePropertyValue(*property, value, serializedValue, context))
		{
			SetError(outError, ReflectionErrorCode::InvalidPropertyValue,
				property->GetPath(), "Failed to encode a serializable property.");
			return false;
		}

		// Add the serialized property value to the JSON object using the property's serialized name as the key.
		if (!SetValueAtPath(result, property->GetPath(), std::move(serializedValue)))
		{
			SetError(outError, ReflectionErrorCode::InvalidMetadata,
				property->GetPath(), "Property path conflicts with another serialized property.");
			return false;
		}
	}

	outJson = std::move(result);
	return true;
}

bool ReflectionDeserializer::Deserialize(
	const TypeMetadata& metadata,
	std::type_index objectType,
	const nlohmann::json& json,
	void* object,
	ReflectionRestoreContext context,
	ReflectionError* outError)
{
	if (!object || objectType != metadata.GetType())
	{
		SetError(outError, ReflectionErrorCode::InvalidObject, std::nullopt,
			"Object type does not match its reflection metadata.");
		return false;
	}

	if (!json.is_object())
	{
		SetError(outError, ReflectionErrorCode::SchemaMismatch, std::nullopt,
			"Reflection data must be a JSON object.");
		return false;
	}

	const auto properties = GetSerializableProperties(metadata);
	PropertySchemaNode schema;
	const bool schemaBuilt = BuildSchema(properties, schema);
	const bool schemaValid = schemaBuilt && ValidateSchema(json, schema);
	if (!schemaValid)
	{
		SetError(outError, ReflectionErrorCode::SchemaMismatch, std::nullopt,
			"JSON members do not match the registered reflection schema.");
		return false;
	}

	// The lists of a pair of property metadata and deserialized property value to be written.
	std::vector<std::pair<const PropertyMetadata*, PropertyValue>> values;
	values.reserve(properties.size());

	for (const PropertyMetadata* property : properties)
	{
		// Find the JSON entry by using the serialized name as the key.
		const nlohmann::json* entry = FindValueAtPath(json, property->GetPath());
		if (!entry)
		{
			const SerializationMetadata* serialization = property->GetSerializationMetadata();
			if (serialization && serialization->requirement == PropertyRequirement::Optional) continue;
			SetError(outError, ReflectionErrorCode::MissingProperty,
				property->GetPath(), "A required property is missing.");
			return false;
		}

		PropertyValue value;

		// Deserialize the JSON entry into a PropertyValue and validate it against the property's constraints.
		if (!DeserializePropertyValue(*property, *entry, value, context))
		{
			SetError(outError, ReflectionErrorCode::InvalidPropertyValue,
				property->GetPath(), "Serialized property has an invalid representation.");
			return false;
		}

		if (!property->ValidateValue(value))
		{
			SetError(outError, ReflectionErrorCode::InvalidPropertyValue,
				property->GetPath(), "Serialized property is outside its supported value domain.");
			return false;
		}

		values.emplace_back(property, std::move(value));
	}

	std::vector<std::pair<const PropertyMetadata*, PropertyValue>> previousValues;
	previousValues.reserve(values.size());
	for (const auto& propertyAndValue : values)
	{
		const PropertyMetadata* property = propertyAndValue.first;
		PropertyValue previousValue;
		if (!property->Read(objectType, object, previousValue))
		{
			SetError(outError, ReflectionErrorCode::PropertyReadFailed,
				property->GetPath(), "Failed to capture a property before deserialization.");
			return false;
		}
		previousValues.emplace_back(property, std::move(previousValue));
	}

	std::size_t appliedCount = 0;
	auto Rollback = [&]()
	{
		bool restored = true;
		while (appliedCount > 0)
		{
			--appliedCount;
			const auto& previous = previousValues[appliedCount];
			if (!previous.first->Write(objectType, object, previous.second)) restored = false;
		}
		return restored;
	};

	for (const auto& propertyAndValue : values)
	{
		const PropertyMetadata* property = propertyAndValue.first;
		++appliedCount;
		if (!property->Write(objectType, object, propertyAndValue.second))
		{
			const bool restored = Rollback();
			ReflectionErrorCode code = ReflectionErrorCode::PropertyWriteFailed;
			if (!restored) code = ReflectionErrorCode::RollbackFailed;
			SetError(outError, code, property->GetPath(),
				"Failed to apply a deserialized property.");
			return false;
		}
	}

	ReflectionError validationError;
	if (!metadata.Validate(objectType, object, &validationError))
	{
		const bool restored = Rollback();
		if (!restored)
		{
			SetError(outError, ReflectionErrorCode::RollbackFailed,
				validationError.path, "Type validation failed and rollback was incomplete.");
			return false;
		}

		if (outError) *outError = std::move(validationError);
		return false;
	}

	return true;
}
