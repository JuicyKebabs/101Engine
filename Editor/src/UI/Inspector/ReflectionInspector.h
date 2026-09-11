#pragma once

#include "Engine/Core/Reflection/PropertyMetadata.h"
#include <functional>
#include <optional>
#include <typeindex>
#include <vector>

enum class ReflectionInspectorPolicy
{
	Editable,
	ReadOnly,
};

enum class PropertyEditorKind
{
	Unsupported,
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

struct PropertyInspectorRow
{
	const PropertyMetadata* property = nullptr;
	PropertyEditorKind editor = PropertyEditorKind::Unsupported;
	bool editable = false;
};

struct ReflectionInspectorCallbacks
{
	std::function<void(const PropertyMetadata&, const PropertyValue&)> onEditBegin;
	std::function<bool(const PropertyMetadata&, const PropertyValue&, const PropertyValue&)> onEditCommit;
	std::function<void(const PropertyMetadata&, const PropertyValue&, bool)> onEditCancel;
	std::function<bool(const PropertyMetadata&, const PropertyValue&)> restoreValue;
};

struct ReflectionInspectorServices
{
	std::function<bool(
		const char*, const PropertyMetadata&, const ActorReference&, ActorReference&)> drawActorReference;
	std::function<bool(
		const char*, const PropertyMetadata&, const AssetReferenceValue&, AssetReferenceValue&)> drawAssetReference;
};

struct ReflectionInspectorResult
{
	std::size_t displayedProperties = 0;
	std::size_t readFailures = 0;
	std::size_t unsupportedProperties = 0;
	std::size_t writeFailures = 0;
	std::size_t restoreFailures = 0;
};

class ReflectionInspector
{
public:
	static PropertyEditorKind GetEditorKind(PropertyLogicalType type);
	static std::vector<PropertyInspectorRow> BuildRows(
		const TypeMetadata& metadata,
		ReflectionInspectorPolicy policy);

	ReflectionInspectorResult Draw(
		const TypeMetadata& metadata,
		std::type_index objectType,
		void* object,
		ReflectionInspectorPolicy policy,
		const ReflectionInspectorCallbacks& callbacks = {},
		const ReflectionInspectorServices& services = {});

	bool BeginEdit(
		const TypeMetadata& metadata,
		std::type_index objectType,
		void* object,
		const PropertyMetadata& property,
		const PropertyValue& before,
		const ReflectionInspectorCallbacks& callbacks = {});
	bool PreviewEdit(
		const TypeMetadata& metadata,
		std::type_index objectType,
		void* object,
		const PropertyMetadata& property,
		const PropertyValue& value,
		const ReflectionInspectorCallbacks& callbacks = {});
	bool CommitEdit(
		const TypeMetadata& metadata,
		std::type_index objectType,
		void* object,
		const PropertyMetadata& property,
		const PropertyValue& after,
		const ReflectionInspectorCallbacks& callbacks = {});
	bool CancelEdit(
		const TypeMetadata& metadata,
		std::type_index objectType,
		void* object,
		const PropertyMetadata& property,
		const ReflectionInspectorCallbacks& callbacks = {});

	bool CancelActiveEdit(const ReflectionInspectorCallbacks& callbacks = {});

private:
	struct Transaction
	{
		const TypeMetadata* metadata = nullptr;
		std::type_index objectType;
		void* object = nullptr;
		const PropertyMetadata* property = nullptr;
		PropertyValue before;
		std::function<bool(const PropertyMetadata&, const PropertyValue&)> restoreValue;
	};

	bool Matches(
		const TypeMetadata& metadata,
		std::type_index objectType,
		const void* object,
		const PropertyMetadata& property) const;
	bool RestoreTransaction(const Transaction& transaction) const;

	std::optional<Transaction> m_transaction;
};
