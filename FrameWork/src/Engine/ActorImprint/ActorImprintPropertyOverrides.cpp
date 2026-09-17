#include "ActorImprintPropertyOverrides.h"
#include "Engine/Core/Debug/Debug.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

namespace
{
	using json = nlohmann::json;

	struct SchemaNode
	{
		const PropertyMetadata* property = nullptr;
		std::map<std::string, SchemaNode> children;
	};

	bool BuildSchema(const TypeMetadata& metadata, SchemaNode& root)
	{
		for (const auto& property : metadata.GetProperties())
		{
			if (!property.GetSerializationMetadata())
			{
				continue;
			}

			SchemaNode* node = &root;

			for (const std::string& member : property.GetPath().GetMembers())
			{
				if (node->property)
				{
					return false;
				}

				node = &node->children[member];
			}

			if (node->property || !node->children.empty())
			{
				return false;
			}

			node->property = &property;
		}

		return true;
	}

	bool ValidateSchema(const json& source, const SchemaNode& schema)
	{
		if (!source.is_object())
		{
			return false;
		}

		for (auto member = source.begin(); member != source.end(); ++member)
		{
			if (!schema.children.contains(member.key()))
			{
				return false;
			}
		}

		for (const auto& [name, child] : schema.children)
		{
			const auto value = source.find(name);

			if (value == source.end())
			{
				return false;
			}

			if (!child.property && !ValidateSchema(*value, child))
			{
				return false;
			}
		}

		return true;
	}

	bool ValidateNormalizedObject(const TypeMetadata& metadata, const json& source)
	{
		SchemaNode schema;

		if (!BuildSchema(metadata, schema))
		{
			return false;
		}

		return ValidateSchema(source, schema);
	}

	const json* FindValue(const json& root, const PropertyPath& path)
	{
		const json* value = &root;

		for (const std::string& member : path.GetMembers())
		{
			if (!value->is_object())
			{
				return nullptr;
			}

			const auto child = value->find(member);

			if (child == value->end())
			{
				return nullptr;
			}

			value = &*child;
		}

		return value;
	}

	enum class PathLookup { Found, Missing, ArrayElement };

	PathLookup InspectPath(const json& root, const PropertyPath& path)
	{
		const json* value = &root;

		for (const std::string& member : path.GetMembers())
		{
			if (value->is_array())
			{
				return PathLookup::ArrayElement;
			}

			if (!value->is_object())
			{
				return PathLookup::Missing;
			}

			const auto child = value->find(member);

			if (child == value->end())
			{
				return PathLookup::Missing;
			}

			value = &*child;
		}

		return PathLookup::Found;
	}

	bool ReplaceValue(json& root, const PropertyPath& path, json value)
	{
		json* object = &root;
		const auto& members = path.GetMembers();

		for (std::size_t i = 0; i + 1 < members.size(); ++i)
		{
			const auto child = object->find(members[i]);

			if (child == object->end() || !child->is_object())
			{
				return false;
			}

			object = &*child;
		}

		const auto destination = object->find(members.back());

		if (destination == object->end())
		{
			return false;
		}

		*destination = std::move(value);
		return true;
	}

	bool IsAncestor(const PropertyPath& ancestor, const PropertyPath& descendant)
	{
		const auto& left = ancestor.GetMembers();
		const auto& right = descendant.GetMembers();
		return left.size() < right.size() && std::equal(left.begin(), left.end(), right.begin());
	}

	bool IsPersistentJsonValue(const json& value)
	{
		if (value.is_null() || value.is_boolean() || value.is_string() ||
			value.is_number_integer() || value.is_number_unsigned())
		{
			return true;
		}

		if (value.is_number_float())
		{
			return std::isfinite(value.get<double>());
		}

		if (value.is_array())
		{
			return std::all_of(value.begin(), value.end(), IsPersistentJsonValue);
		}

		if (value.is_object())
		{
			return std::all_of(value.begin(), value.end(), [](const auto& member)
			{
				return IsPersistentJsonValue(member);
			});
		}

		return false;
	}

	bool IsStrictlyEncodableJson(const json& value)
	{
		try
		{
			(void)value.dump();
			return true;
		}
		catch (const json::exception&)
		{
			return false;
		}
	}

	enum class RepresentationCompatibility { Compatible, Incompatible, InvalidMetadata };

	RepresentationCompatibility IsRepresentationCompatible(const PropertyMetadata& property, const json& value)
	{
		switch (property.GetLogicalType())
		{
		case PropertyLogicalType::Bool:
			return value.is_boolean() ? RepresentationCompatibility::Compatible
									  : RepresentationCompatibility::Incompatible;
		case PropertyLogicalType::SignedInteger:
		case PropertyLogicalType::UnsignedInteger:
			return value.is_number_integer() ? RepresentationCompatibility::Compatible
											 : RepresentationCompatibility::Incompatible;
		case PropertyLogicalType::Float:
		case PropertyLogicalType::Double:
			return value.is_number_float() ? RepresentationCompatibility::Compatible
										   : RepresentationCompatibility::Incompatible;
		case PropertyLogicalType::String:
			return value.is_string() ? RepresentationCompatibility::Compatible
									 : RepresentationCompatibility::Incompatible;
		case PropertyLogicalType::Vector2:
		case PropertyLogicalType::Vector3:
		case PropertyLogicalType::Vector4:
		case PropertyLogicalType::Quaternion:
			return value.is_array() ? RepresentationCompatibility::Compatible
									: RepresentationCompatibility::Incompatible;
		case PropertyLogicalType::Enum:
		{
			const auto format = property.GetEnumSerializationFormat();

			if (!format || !property.GetEnumMetadata())
			{
				return RepresentationCompatibility::InvalidMetadata;
			}

			const bool compatible = *format == EnumSerializationFormat::Name
				? value.is_string() : value.is_number_integer();
			return compatible ? RepresentationCompatibility::Compatible : RepresentationCompatibility::Incompatible;
		}
		case PropertyLogicalType::ActorReference:
			return value.is_null() || value.is_object()
				? RepresentationCompatibility::Compatible : RepresentationCompatibility::Incompatible;
		case PropertyLogicalType::AssetReference:
			if (property.GetAssetType() == AssetType::Unknown)
			{
				return RepresentationCompatibility::InvalidMetadata;
			}

			return value.is_null() || value.is_string()
				? RepresentationCompatibility::Compatible : RepresentationCompatibility::Incompatible;
		default:
			return RepresentationCompatibility::InvalidMetadata;
		}
	}

	bool NormalizeEntries(std::vector<ActorImprintPropertyOverrideEntry>& properties)
	{
		std::sort(properties.begin(), properties.end(), [](const auto& left, const auto& right)
		{
			return left.path.ToString() < right.path.ToString();
		});

		for (std::size_t i = 0; i < properties.size(); ++i)
		{
			if (!IsPersistentJsonValue(properties[i].value) ||
				!IsStrictlyEncodableJson(properties[i].value))
			{
				DBG("Override value must be finite standard JSON.");
				return false;
			}

			for (std::size_t j = 0; j < i; ++j)
			{
				if (properties[j].path == properties[i].path)
				{
					DBG("Override path is duplicated.");
					return false;
				}

				if (IsAncestor(properties[j].path, properties[i].path) ||
					IsAncestor(properties[i].path, properties[j].path))
				{
					DBG("Ancestor and descendant override paths cannot coexist.");
					return false;
				}
			}
		}

		return true;
	}
}

bool ActorImprintPropertyOverrides::ValidateStructure(
	const ActorImprintPropertyOverrideTarget& overrides)
{
	if (overrides.targetLocalObjectId == InvalidLocalObjectId)
	{
		DBG("Override target LocalObjectID must be nonzero.");
		return false;
	}

	try
	{
		auto properties = overrides.properties;
		return NormalizeEntries(properties);
	}
	catch (const std::exception& exception)
	{
		DBG("%s", exception.what());
		return false;
	}
}

bool ActorImprintPropertyOverrides::Diff(
	const TypeMetadata& metadata,
	LocalObjectId targetLocalObjectId,
	const nlohmann::json& normalizedDefault,
	const nlohmann::json& normalizedCurrent,
	ActorImprintPropertyOverrideTarget& outTarget)
{
	if (targetLocalObjectId == InvalidLocalObjectId)
	{
		DBG("Override target LocalObjectID must be nonzero.");
		return false;
	}

	try
	{
		if (!ValidateNormalizedObject(metadata, normalizedDefault))
		{
			DBG("Normalized reflected properties are invalid.");
			return false;
		}

		if (!ValidateNormalizedObject(metadata, normalizedCurrent))
		{
			DBG("Normalized reflected properties are invalid.");
			return false;
		}

		std::vector<const PropertyMetadata*> properties;

		for (const auto& property : metadata.GetProperties())
		{
			if (property.GetSerializationMetadata())
			{
				properties.push_back(&property);
			}
		}

		std::sort(properties.begin(), properties.end(), [](const auto* left, const auto* right)
		{
			return left->GetPath().ToString() < right->GetPath().ToString();
		});

		ActorImprintPropertyOverrideTarget result;
		result.targetLocalObjectId = targetLocalObjectId;

		for (const PropertyMetadata* property : properties)
		{
			const json* defaultValue = FindValue(normalizedDefault, property->GetPath());
			const json* currentValue = FindValue(normalizedCurrent, property->GetPath());

			if (!defaultValue)
			{
				DBG("Normalized default property is missing.");
				return false;
			}

			if (!currentValue)
			{
				DBG("Normalized current property is missing.");
				return false;
			}

			if (*defaultValue != *currentValue)
			{
				result.properties.emplace_back(property->GetPath(), *currentValue);
			}
		}

		outTarget = std::move(result);
		return true;
	}
	catch (const std::exception& exception)
	{
		DBG("%s", exception.what());
		return false;
	}
}

bool ActorImprintPropertyOverrides::Merge(
	const TypeMetadata& metadata,
	const nlohmann::json& normalizedDefault,
	const ActorImprintPropertyOverrideTarget& overrides,
	ActorImprintOverrideRevisionRelation revisions,
	nlohmann::json& outCompleted,
	std::vector<ActorImprintPropertyOverrideEntry>* outApplied,
	ActorImprintPropertyOverrideMigration* outMigration)
{
	if (overrides.targetLocalObjectId == InvalidLocalObjectId)
	{
		DBG("Override target LocalObjectID must be nonzero.");
		return false;
	}

	try
	{
		if (!ValidateNormalizedObject(metadata, normalizedDefault))
		{
			DBG("Normalized reflected properties are invalid.");
			return false;
		}

		if (!ValidateStructure(overrides))
		{
			return false;
		}

		auto properties = overrides.properties;
		std::sort(properties.begin(), properties.end(), [](const auto& left, const auto& right)
		{
			return left.path.ToString() < right.path.ToString();
		});
		json completed = normalizedDefault;
		std::vector<ActorImprintPropertyOverrideEntry> applied;
		ActorImprintPropertyOverrideMigration migration;

		for (const auto& propertyOverride : properties)
		{
			const auto lookup = InspectPath(normalizedDefault, propertyOverride.path);

			if (lookup == PathLookup::ArrayElement)
			{
				DBG("Property Override cannot address an array element.");
				return false;
			}

			const PropertyMetadata* property = metadata.FindPropertyByPath(propertyOverride.path);

			if (!property || !property->GetSerializationMetadata() || lookup != PathLookup::Found)
			{
				if (revisions == ActorImprintOverrideRevisionRelation::Different)
				{
					++migration.staleMissingPath;
					continue;
				}
			{
				DBG("Override path does not identify an existing reflected property.");
				return false;
			}
			}

			const auto compatibility = IsRepresentationCompatible(*property, propertyOverride.value);

			if (compatibility == RepresentationCompatibility::InvalidMetadata)
			{
				DBG("Reflected property has invalid serialization metadata.");
				return false;
			}

			if (compatibility == RepresentationCompatibility::Incompatible)
			{
				if (revisions == ActorImprintOverrideRevisionRelation::Different)
				{
					++migration.staleIncompatible;
					continue;
				}
			{
				DBG("Override JSON representation is incompatible with the reflected property.");
				return false;
			}
			}

			if (!ReplaceValue(completed, propertyOverride.path, propertyOverride.value))
			{
				DBG("Normalized default no longer contains the reflected property.");
				return false;
			}

			applied.push_back(propertyOverride);
			++migration.applied;
		}

		outCompleted = std::move(completed);

		if (outApplied)
		{
			*outApplied = std::move(applied);
		}

		if (outMigration)
		{
			*outMigration = migration;
		}

		return true;
	}
	catch (const std::exception& exception)
	{
		DBG("%s", exception.what());
		return false;
	}
}
