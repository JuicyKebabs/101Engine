#include "PropertyMetadata.h"
#include "Engine/Core/Debug/Debug.h"

const EnumEntry* EnumMetadata::FindByName(std::string_view serializedName) const
{
	for (const EnumEntry& entry : m_entries)
	{
		if (entry.serializedName == serializedName)
		{
			return &entry;
		}
	}

	return nullptr;
}

const EnumEntry* EnumMetadata::FindByValue(std::int64_t value) const
{
	for (const EnumEntry& entry : m_entries)
	{
		if (entry.value == value)
		{
			return &entry;
		}
	}

	return nullptr;
}

bool PropertyMetadata::Read(
	std::type_index objectType,
	const void* object,
	PropertyValue& outValue) const
{
	if (!object)
	{
		return false;
	}

	if (objectType != m_objectType || !m_read)
	{
		return false;
	}

	PropertyValue candidate;

	if (!m_read(object, candidate))
	{
		return false;
	}

	const bool isEnum = m_logicalType == PropertyLogicalType::Enum;

	if (isEnum && !IsRegisteredEnumValue(candidate))
	{
		return false;
	}

	outValue = std::move(candidate);
	return true;
}

bool PropertyMetadata::Write(
	std::type_index objectType,
	void* object,
	const PropertyValue& value) const
{
	if (!object)
	{
		return false;
	}

	if (objectType != m_objectType || !m_write)
	{
		return false;
	}

	if (!ValidateValue(value))
	{
		return false;
	}

	return m_write(object, value);
}

bool PropertyMetadata::ValidateValue(const PropertyValue& value) const
{
	if (!m_valueValidator || !m_valueValidator(value))
	{
		return false;
	}

	return m_logicalType != PropertyLogicalType::Enum || IsRegisteredEnumValue(value);
}

bool PropertyMetadata::IsValid() const
{
	const bool hasIdentity = !m_serializedName.empty()
		&& m_logicalType != PropertyLogicalType::Invalid;
	const bool hasValidAccessors = m_valueValidator && m_read && m_write;

	if (!hasIdentity || !hasValidAccessors)
	{
		return false;
	}

	if (m_logicalType == PropertyLogicalType::Enum)
	{
		const bool hasMatchingEnumType = m_enumMetadata
			&& m_enumMetadata->GetType() == m_valueType;
		const bool hasSerializationFormat = !m_serialization || m_serialization->enumFormat.has_value();
		const bool hasNoAssetType = m_assetType == AssetType::Unknown;
		return hasMatchingEnumType && hasSerializationFormat && hasNoAssetType;
	}

	if (m_logicalType == PropertyLogicalType::AssetReference)
	{
		return !m_enumMetadata && m_assetType != AssetType::Unknown;
	}

	const bool hasNoEnumMetadata = !m_enumMetadata && !GetEnumSerializationFormat();
	const bool hasNoAssetType = m_assetType == AssetType::Unknown;
	return hasNoEnumMetadata && hasNoAssetType;
}

bool PropertyMetadata::IsRegisteredEnumValue(const PropertyValue& value) const
{
	const EnumPropertyValue* enumValue = std::get_if<EnumPropertyValue>(&value);
	return enumValue && m_enumMetadata && m_enumMetadata->FindByValue(enumValue->value);
}

const PropertyMetadata* TypeMetadata::FindProperty(std::string_view serializedName) const
{
	const auto path = PropertyPath::FromMembers({std::string(serializedName)});

	if (!path)
	{
		return nullptr;
	}

	return FindPropertyByPath(*path);
}

const PropertyMetadata* TypeMetadata::FindPropertyByPath(const PropertyPath& path) const
{
	for (const PropertyMetadata& property : m_properties)
	{
		if (property.GetPath() == path)
		{
			return &property;
		}
	}

	return nullptr;
}

bool TypeMetadata::Validate(std::type_index objectType, const void* object) const
{
	if (!object || objectType != m_type)
	{
		DBG("Object type does not match its reflection metadata.");
		return false;
	}

	return !m_validator || m_validator(objectType, object);
}

bool TypeMetadata::TryWriteProperty(
	std::type_index objectType,
	void* object,
	const PropertyMetadata& property,
	const PropertyValue& value) const
{
	if (FindPropertyByPath(property.GetPath()) != &property)
	{
		DBG("Property does not belong to this type metadata.");
		return false;
	}

	PropertyValue before;

	if (!property.Read(objectType, object, before))
	{
		DBG("Failed to capture the property value before editing.");
		return false;
	}

	if (!property.Write(objectType, object, value))
	{
		DBG("Property rejected the edited value.");

		if (!property.Write(objectType, object, before))
		{
			DBG("Property edit rollback failed.");
		}

		return false;
	}

	if (Validate(objectType, object))
	{
		return true;
	}

	if (!property.Write(objectType, object, before))
	{
		DBG("Property edit rollback failed.");
	}

	return false;
}

bool TypeMetadata::CopySerializableState(
	std::type_index objectType,
	const void* source,
	void* destination) const
{
	if (!source || !destination || objectType != m_type)
	{
		DBG("Source or destination does not match its reflection metadata.");
		return false;
	}

	struct StateEntry
	{
		const PropertyMetadata* property = nullptr;
		PropertyValue sourceValue;
		PropertyValue destinationValue;
	};
	std::vector<StateEntry> entries;

	for (const PropertyMetadata& property : m_properties)
	{
		if (!property.GetSerializationMetadata())
		{
			continue;
		}

		StateEntry entry;
		entry.property = &property;
		const bool sourceRead = property.Read(objectType, source, entry.sourceValue);
		const bool destinationRead = property.Read(objectType, destination, entry.destinationValue);

		if (!sourceRead || !destinationRead)
		{
			DBG("Failed to capture reflected component state.");
			return false;
		}

		entries.push_back(std::move(entry));
	}

	std::size_t appliedCount = 0;
	auto rollback = [&]()
	{
		bool restored = true;
		while (appliedCount > 0)
		{
			const StateEntry& entry = entries[--appliedCount];

			if (!entry.property->Write(objectType, destination, entry.destinationValue))
			{
				restored = false;
			}
		}

		if (!restored)
		{
			DBG("Reflected component state rollback failed.");
		}
	};

	for (const StateEntry& entry : entries)
	{
		++appliedCount;

		if (entry.property->Write(objectType, destination, entry.sourceValue))
		{
			continue;
		}

		DBG("Failed to copy reflected component state.");
		rollback();
		return false;
	}

	if (Validate(objectType, destination))
	{
		return true;
	}

	rollback();
	return false;
}
