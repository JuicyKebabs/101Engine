#include "UI/Inspector/ReflectionInspector.h"
#include "Engine/Component/Transform.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"

#include <iostream>
#include <memory>

namespace
{
	int failures = 0;
	void Check(bool value, const char* name)
	{
		if (value)
		{
			std::cout << "[PASS] " << name << '\n';
			return;
		}

		std::cerr << "[FAIL] " << name << '\n';
		++failures;
	}

	struct Object
	{
		int value = 1;
		int locked = 2;
	};

	void TestRowsAndTransaction()
	{
		TypeMetadataBuilder<Object> builder("InspectorObject");
		builder.Property("value", &Object::value);
		builder.Property("locked", &Object::locked).Inspector(InspectorMetadata{.readOnly = true});
		auto metadata = builder.Build();
		Check(metadata.has_value(), "Inspector metadata builds");
		if (!metadata) return;

		auto editable = ReflectionInspector::BuildRows(*metadata, ReflectionInspectorPolicy::Editable);
		auto readOnly = ReflectionInspector::BuildRows(*metadata, ReflectionInspectorPolicy::ReadOnly);
		Check(editable.size() == 2 && editable[0].editable && !editable[1].editable,
			"Rows honor property EditorReadOnly policy");
		Check(readOnly.size() == 2 && !readOnly[0].editable && !readOnly[1].editable,
			"Overall ReadOnly policy disables every row");

		Object object;
		ReflectionInspector inspector;
		const PropertyMetadata& property = *metadata->FindProperty("value");
		int commits = 0;
		ReflectionInspectorCallbacks callbacks;
		callbacks.onEditCommit = [&commits](const auto&, const auto&, const auto&) { ++commits; return true; };
		Check(inspector.BeginEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 1 }),
			"Transaction begins with the original value");
		Check(inspector.PreviewEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 5 }) && object.value == 5,
			"Preview writes through PropertyMetadata");
		Check(inspector.CommitEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 5 }, callbacks) && commits == 1,
			"Commit notifies exactly once");

		Check(inspector.BeginEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 5 }) &&
			inspector.PreviewEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 9 }) &&
			inspector.CancelActiveEdit() && object.value == 5,
			"Cancel restores through PropertyMetadata");
	}

	void TestDispatch()
	{
		Check(ReflectionInspector::GetEditorKind(PropertyLogicalType::Bool) == PropertyEditorKind::Bool &&
			ReflectionInspector::GetEditorKind(PropertyLogicalType::Enum) == PropertyEditorKind::Enum &&
			ReflectionInspector::GetEditorKind(PropertyLogicalType::ActorReference) == PropertyEditorKind::ActorReference &&
			ReflectionInspector::GetEditorKind(PropertyLogicalType::AssetReference) == PropertyEditorKind::AssetReference,
			"Logical types use the explicit editor dispatcher");
	}

	void TestStableRestoreDoesNotUseDestroyedObject()
	{
		TypeMetadataBuilder<Object> builder("StableRestoreObject");
		builder.Property("value", &Object::value);
		auto metadata = builder.Build();
		Check(metadata.has_value(), "Stable restore metadata builds");
		if (!metadata)
		{
			return;
		}

		auto editedObject = std::make_unique<Object>();
		Object stableTarget;
		const PropertyMetadata& property = *metadata->FindProperty("value");

		ReflectionInspectorCallbacks callbacks;
		callbacks.restoreValue = [&stableTarget](
			const PropertyMetadata& stableProperty,
			const PropertyValue& value)
		{
			return stableProperty.Write(typeid(Object), &stableTarget, value);
		};

		ReflectionInspector inspector;
		const bool began = inspector.BeginEdit(
			*metadata,
			typeid(Object),
			editedObject.get(),
			property,
			std::int64_t{ 1 },
			callbacks);
		const bool previewed = inspector.PreviewEdit(
			*metadata,
			typeid(Object),
			editedObject.get(),
			property,
			std::int64_t{ 7 });
		editedObject.reset();

		const bool canceled = inspector.CancelActiveEdit();
		Check(began && previewed && canceled && stableTarget.value == 1,
			"Cancel resolves a stable target after the preview object is destroyed");
	}
}

void TestTransformPilot()
{
	const auto* metadata = ComponentRegistry::Get().GetMetadata(typeid(Transform));
	Check(metadata != nullptr, "Transform pilot metadata is registered");
	if (!metadata) return;
	Transform transform;
	const auto* position = metadata->FindProperty("position");
	ReflectionInspector inspector;
	const Vector3 edited{4.0f, 5.0f, 6.0f};
	Check(inspector.BeginEdit(*metadata, typeid(Transform), &transform, *position, Vector3::Zero()),
		"Transform position Inspector edit begins");
	Check(inspector.PreviewEdit(*metadata, typeid(Transform), &transform, *position, edited) &&
		transform.GetLocalPosition().x == 4.0f, "Inspector invokes inferred Transform setter");
	Check(inspector.CancelEdit(*metadata, typeid(Transform), &transform, *position) &&
		transform.GetLocalPosition().x == 0.0f, "Inspector cancel restores Transform position");
}

void TestColorPresentation()
{
	struct Colors { Vector4 vector{0.1f, 0.2f, 0.3f, 1.0f}; Vector4 color = vector; };
	TypeMetadataBuilder<Colors> builder("Colors");
	builder.Property("vector", &Colors::vector);
	builder.Property("color", &Colors::color).Inspector(InspectorMetadata{.presentation = InspectorPresentation::Color});
	auto metadata = builder.Build();
	Check(metadata.has_value(), "Vector4 and Color presentation metadata builds");
	if (!metadata) return;
	const auto rows = ReflectionInspector::BuildRows(*metadata, ReflectionInspectorPolicy::Editable);
	Check(rows.size() == 2 && rows[0].editor == PropertyEditorKind::Vector4 && rows[1].editor == PropertyEditorKind::Color,
		"Inspector chooses Color presentation independently of the value type");
	Colors object;
	PropertyValue vector, color;
	Check(rows[0].property->Read(typeid(Colors), &object, vector) &&
		rows[1].property->Read(typeid(Colors), &object, color) && vector.index() == color.index(),
		"Color and Vector4 share a single PropertyValue alternative");
	nlohmann::json json;
	Check(ReflectionSerializer::Serialize(*metadata, typeid(Colors), &object, json) && json["vector"] == json["color"],
		"Color presentation does not change the serialized Vector4 representation");
	ReflectionInspector inspector;
	const Vector4 edited{0.8f, 0.7f, 0.6f, 1.0f};
	Check(inspector.BeginEdit(*metadata, typeid(Colors), &object, *rows[1].property, color) &&
		inspector.PreviewEdit(*metadata, typeid(Colors), &object, *rows[1].property, edited) && object.color.x == edited.x &&
		inspector.CancelEdit(*metadata, typeid(Colors), &object, *rows[1].property) && object.color.x == object.vector.x,
		"Color Inspector edits and cancels through the ordinary Vector4 property");
}

int main()
{
	TestColorPresentation();
	TestTransformPilot();
	TestRowsAndTransaction();
	TestDispatch();
	TestStableRestoreDoesNotUseDestroyedObject();
	return failures == 0 ? 0 : 1;
}
