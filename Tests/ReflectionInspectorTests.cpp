#include "UI/Inspector/ReflectionInspector.h"

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
		builder.AddMember("value", &Object::value)
			.AddMember("locked", &Object::locked,
				PropertyPolicy::Serializable | PropertyPolicy::Inspectable | PropertyPolicy::EditorReadOnly);
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
		builder.AddMember("value", &Object::value);
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

int main()
{
	TestRowsAndTransaction();
	TestDispatch();
	TestStableRestoreDoesNotUseDestroyedObject();
	return failures == 0 ? 0 : 1;
}
