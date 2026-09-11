#include "ReflectionInspector.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include <numbers>

namespace
{
	struct PropertyEditorResult
	{
		PropertyValue value;
		bool began = false;
		bool changed = false;
		bool committed = false;
		bool canceled = false;
	};

	struct DisabledScope
	{
		explicit DisabledScope(bool disabled) : active(disabled) { if (active) ImGui::BeginDisabled(); }
		~DisabledScope() { if (active) ImGui::EndDisabled(); }
		bool active;
	};

	bool IsContinuousEditor(PropertyEditorKind kind)
	{
		switch (kind)
		{
		case PropertyEditorKind::Bool:
		case PropertyEditorKind::Enum:
		case PropertyEditorKind::ActorReference:
		case PropertyEditorKind::AssetReference:
			return false;
		default:
			return true;
		}
	}

	float ConvertUnit(float value, NumericUnit source, NumericUnit destination)
	{
		if (source == destination) return value;

		if (source == NumericUnit::Radians && destination == NumericUnit::Degrees)
		{
			return value * 180.0f / std::numbers::pi_v<float>;
		}

		if (source == NumericUnit::Degrees && destination == NumericUnit::Radians)
		{
			return value * std::numbers::pi_v<float> / 180.0f;
		}

		return value;
	}

	bool DrawFloatEditor(
		const char* label,
		float& storedValue,
		const InspectorMetadata* inspector)
	{
		if (!inspector || !inspector->numeric)
		{
			return ImGui::DragFloat(label, &storedValue, 0.05f);
		}

		const NumericEditorMetadata& numeric = *inspector->numeric;
		float displayValue = ConvertUnit(
			storedValue, numeric.storageUnit, numeric.displayUnit);

		float minimum = 0.0f;
		if (numeric.minimum) minimum = static_cast<float>(*numeric.minimum);

		float maximum = 0.0f;
		if (numeric.maximum) maximum = static_cast<float>(*numeric.maximum);

		const bool changed = ImGui::DragFloat(
			label, &displayValue, numeric.dragSpeed, minimum, maximum);
		if (!changed) return false;

		storedValue = ConvertUnit(
			displayValue, numeric.displayUnit, numeric.storageUnit);
		return true;
	}

	const char* GetInspectorLabel(const PropertyMetadata& property)
	{
		const InspectorMetadata* inspector = property.GetInspectorMetadata();
		if (inspector && !inspector->label.empty()) return inspector->label.c_str();
		return property.GetSerializedName().c_str();
	}

	template<class ValueType, class DrawFunction>
	void DrawValue(PropertyValue& value, bool& changed, DrawFunction draw)
	{
		if (ValueType* typed = std::get_if<ValueType>(&value)) changed = draw(*typed);
	}

	PropertyEditorResult DrawPropertyEditor(
		const PropertyInspectorRow& row,
		const PropertyValue& current,
		const ReflectionInspectorServices& services)
	{
		PropertyEditorResult result{ current };
		const PropertyMetadata& property = *row.property;
		const char* label = GetInspectorLabel(property);
		const InspectorMetadata* inspector = property.GetInspectorMetadata();

		DisabledScope disabled(!row.editable);
		switch (row.editor)
		{
		case PropertyEditorKind::Bool:
			DrawValue<bool>(result.value, result.changed,
				[label](bool& value) { return ImGui::Checkbox(label, &value); });
			break;
		case PropertyEditorKind::SignedInteger:
			DrawValue<std::int64_t>(result.value, result.changed,
				[label](std::int64_t& value) { return ImGui::InputScalar(label, ImGuiDataType_S64, &value); });
			break;
		case PropertyEditorKind::UnsignedInteger:
			DrawValue<std::uint64_t>(result.value, result.changed,
				[label](std::uint64_t& value) { return ImGui::InputScalar(label, ImGuiDataType_U64, &value); });
			break;
		case PropertyEditorKind::Float:
			DrawValue<float>(result.value, result.changed,
				[label, inspector](float& value)
				{
					return DrawFloatEditor(label, value, inspector);
				});
			break;
		case PropertyEditorKind::Double:
			DrawValue<double>(result.value, result.changed,
				[label](double& value) { return ImGui::InputDouble(label, &value); });
			break;
		case PropertyEditorKind::String:
			DrawValue<std::string>(result.value, result.changed,
				[label](std::string& value) { return ImGui::InputText(label, &value); });
			break;
		case PropertyEditorKind::Vector2:
			DrawValue<Vector2>(result.value, result.changed,
				[label](Vector2& value) { return ImGui::DragFloat2(label, &value.x, 0.05f); });
			break;
		case PropertyEditorKind::Vector3:
			DrawValue<Vector3>(result.value, result.changed,
				[label](Vector3& value) { return ImGui::DragFloat3(label, &value.x, 0.05f); });
			break;
		case PropertyEditorKind::Vector4:
			DrawValue<Vector4>(result.value, result.changed,
				[label](Vector4& value) { return ImGui::DragFloat4(label, &value.x, 0.05f); });
			break;
		case PropertyEditorKind::Color:
			DrawValue<Vector4>(result.value, result.changed,
				[label](Vector4& value) { return ImGui::ColorEdit4(label, &value.x); });
			break;
		case PropertyEditorKind::Quaternion:
			if (Quaternion* value = std::get_if<Quaternion>(&result.value))
			{
				Vector3 euler = value->ToEulerDeg();
				result.changed = ImGui::DragFloat3(label, &euler.x, 0.25f);
				if (result.changed) *value = Quaternion::CreateFromEulerDeg(euler);
			}
			break;
		case PropertyEditorKind::Enum:
			if (EnumPropertyValue* value = std::get_if<EnumPropertyValue>(&result.value))
			{
				const EnumMetadata* enumMetadata = property.GetEnumMetadata();
				const EnumEntry* selected = nullptr;
				if (enumMetadata) selected = enumMetadata->FindByValue(value->value);

				const char* preview = "<Invalid>";
				if (selected) preview = selected->serializedName.c_str();

				if (enumMetadata && ImGui::BeginCombo(label, preview))
				{
					for (const EnumEntry& entry : enumMetadata->GetEntries())
					{
						if (ImGui::Selectable(entry.serializedName.c_str(), entry.value == value->value))
						{
							value->value = entry.value;
							result.changed = true;
						}
					}
					ImGui::EndCombo();
				}
			}
			break;
		case PropertyEditorKind::ActorReference:
			if (services.drawActorReference)
			{
				if (ActorReference* value = std::get_if<ActorReference>(&result.value))
				{
					ActorReference selected;
					result.changed = services.drawActorReference(label, property, *value, selected);
					if (result.changed) *value = selected;
				}
			}
			else ImGui::TextDisabled("%s: unavailable", label);
			break;
		case PropertyEditorKind::AssetReference:
			if (services.drawAssetReference)
			{
				if (AssetReferenceValue* value = std::get_if<AssetReferenceValue>(&result.value))
				{
					AssetReferenceValue selected;
					result.changed = services.drawAssetReference(label, property, *value, selected);
					if (result.changed) *value = selected;
				}
			}
			else ImGui::TextDisabled("%s: unavailable", label);
			break;
		default:
			ImGui::TextDisabled("%s: unsupported property type", label);
			break;
		}

		if (!row.editable) return result;
		if (IsContinuousEditor(row.editor))
		{
			result.began = ImGui::IsItemActivated();
			result.canceled = ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape);
			result.committed = ImGui::IsItemDeactivatedAfterEdit() && !result.canceled;
		}
		else
		{
			result.began = result.changed;
			result.committed = result.changed;
		}
		return result;
	}
}

PropertyEditorKind ReflectionInspector::GetEditorKind(PropertyLogicalType type)
{
	switch (type)
	{
	case PropertyLogicalType::Bool: return PropertyEditorKind::Bool;
	case PropertyLogicalType::SignedInteger: return PropertyEditorKind::SignedInteger;
	case PropertyLogicalType::UnsignedInteger: return PropertyEditorKind::UnsignedInteger;
	case PropertyLogicalType::Float: return PropertyEditorKind::Float;
	case PropertyLogicalType::Double: return PropertyEditorKind::Double;
	case PropertyLogicalType::String: return PropertyEditorKind::String;
	case PropertyLogicalType::Vector2: return PropertyEditorKind::Vector2;
	case PropertyLogicalType::Vector3: return PropertyEditorKind::Vector3;
	case PropertyLogicalType::Vector4: return PropertyEditorKind::Vector4;
	case PropertyLogicalType::Quaternion: return PropertyEditorKind::Quaternion;
	case PropertyLogicalType::Enum: return PropertyEditorKind::Enum;
	case PropertyLogicalType::ActorReference: return PropertyEditorKind::ActorReference;
	case PropertyLogicalType::AssetReference: return PropertyEditorKind::AssetReference;
	default: return PropertyEditorKind::Unsupported;
	}
}

std::vector<PropertyInspectorRow> ReflectionInspector::BuildRows(
	const TypeMetadata& metadata,
	ReflectionInspectorPolicy policy)
{
	std::vector<PropertyInspectorRow> rows;
	for (const PropertyMetadata& property : metadata.GetProperties())
	{
		const InspectorMetadata* inspector = property.GetInspectorMetadata();
		if (!inspector) continue;
		const PropertyEditorKind editor = property.GetLogicalType() == PropertyLogicalType::Vector4 &&
			inspector->presentation == InspectorPresentation::Color
			? PropertyEditorKind::Color : GetEditorKind(property.GetLogicalType());
		const bool supported = editor != PropertyEditorKind::Unsupported;
		const bool editablePolicy = policy == ReflectionInspectorPolicy::Editable;
		rows.push_back({
			&property,
			editor,
			editablePolicy && supported && !inspector->readOnly
		});
	}
	return rows;
}

ReflectionInspectorResult ReflectionInspector::Draw(
	const TypeMetadata& metadata,
	std::type_index objectType,
	void* object,
	ReflectionInspectorPolicy policy,
	const ReflectionInspectorCallbacks& callbacks,
	const ReflectionInspectorServices& services)
{
	ReflectionInspectorResult summary;
	for (const PropertyInspectorRow& row : BuildRows(metadata, policy))
	{
		++summary.displayedProperties;
		if (row.editor == PropertyEditorKind::Unsupported) ++summary.unsupportedProperties;

		PropertyValue current;
		if (!row.property->Read(objectType, object, current))
		{
			++summary.readFailures;
			ImGui::TextDisabled("%s: read failed", row.property->GetPath().ToString().c_str());
			continue;
		}

		ImGui::PushID(row.property->GetPath().ToString().c_str());
		const PropertyEditorResult edit = DrawPropertyEditor(row, current, services);
		ImGui::PopID();
		if (!row.editable) continue;

		if (edit.began && !Matches(metadata, objectType, object, *row.property))
		{
			BeginEdit(metadata, objectType, object, *row.property, current, callbacks);
		}
		bool previewSucceeded = true;
		if (edit.changed)
		{
			previewSucceeded = PreviewEdit(
				metadata, objectType, object, *row.property, edit.value, callbacks);
		}
		if (!previewSucceeded)
		{
			++summary.writeFailures;
			const bool restored = CancelEdit(
				metadata, objectType, object, *row.property, callbacks);
			if (!restored)
			{
				++summary.restoreFailures;
			}
			continue;
		}
		if (edit.canceled)
		{
			const bool restored = CancelEdit(
				metadata, objectType, object, *row.property, callbacks);
			if (!restored)
			{
				++summary.restoreFailures;
			}
		}
		else if (edit.committed)
		{
			const bool committed = CommitEdit(
				metadata, objectType, object, *row.property, edit.value, callbacks);
			if (!committed)
			{
				++summary.restoreFailures;
			}
		}
	}
	return summary;
}

bool ReflectionInspector::BeginEdit(
	const TypeMetadata& metadata,
	std::type_index objectType,
	void* object,
	const PropertyMetadata& property,
	const PropertyValue& before,
	const ReflectionInspectorCallbacks& callbacks)
{
	if (!object || property.GetPath().GetMembers().empty())
	{
		return false;
	}
	const bool conflictsWithActiveEdit = m_transaction
		&& !Matches(metadata, objectType, object, property);
	if (conflictsWithActiveEdit)
	{
		return false;
	}
	if (!m_transaction)
	{
		m_transaction = Transaction{
			&metadata,
			objectType,
			object,
			&property,
			before,
			callbacks.restoreValue };
		if (callbacks.onEditBegin) callbacks.onEditBegin(property, before);
	}
	return true;
}

bool ReflectionInspector::PreviewEdit(
	const TypeMetadata& metadata,
	std::type_index objectType,
	void* object,
	const PropertyMetadata& property,
	const PropertyValue& value,
	const ReflectionInspectorCallbacks&)
{
	if (!Matches(metadata, objectType, object, property)) return false;
	return metadata.TryWriteProperty(objectType, object, property, value);
}

bool ReflectionInspector::CommitEdit(
	const TypeMetadata& metadata,
	std::type_index objectType,
	void* object,
	const PropertyMetadata& property,
	const PropertyValue& after,
	const ReflectionInspectorCallbacks& callbacks)
{
	if (!Matches(metadata, objectType, object, property)) return false;
	const PropertyValue before = m_transaction->before;
	m_transaction.reset();
	const bool commandRecorded = !callbacks.onEditCommit
		|| callbacks.onEditCommit(property, before, after);
	if (commandRecorded)
	{
		return true;
	}
	return metadata.TryWriteProperty(objectType, object, property, before);
}

bool ReflectionInspector::CancelEdit(
	const TypeMetadata& metadata,
	std::type_index objectType,
	void* object,
	const PropertyMetadata& property,
	const ReflectionInspectorCallbacks& callbacks)
{
	if (!Matches(metadata, objectType, object, property)) return false;
	const PropertyValue before = m_transaction->before;
	const Transaction transaction = *m_transaction;
	const bool restored = RestoreTransaction(transaction);
	m_transaction.reset();
	if (callbacks.onEditCancel) callbacks.onEditCancel(property, before, restored);
	return restored;
}

bool ReflectionInspector::CancelActiveEdit(const ReflectionInspectorCallbacks& callbacks)
{
	if (!m_transaction) return true;
	const Transaction transaction = *m_transaction;
	m_transaction.reset();
	const bool restored = RestoreTransaction(transaction);
	if (callbacks.onEditCancel)
		callbacks.onEditCancel(*transaction.property, transaction.before, restored);
	return restored;
}

bool ReflectionInspector::Matches(
	const TypeMetadata& metadata,
	std::type_index objectType,
	const void* object,
	const PropertyMetadata& property) const
{
	if (!m_transaction) return false;
	if (m_transaction->metadata != &metadata) return false;
	if (m_transaction->objectType != objectType) return false;
	if (m_transaction->object != object) return false;
	return m_transaction->property->GetPath() == property.GetPath();
}

bool ReflectionInspector::RestoreTransaction(const Transaction& transaction) const
{
	if (transaction.restoreValue)
	{
		return transaction.restoreValue(*transaction.property, transaction.before);
	}

	return transaction.metadata->TryWriteProperty(
		transaction.objectType,
		transaction.object,
		*transaction.property,
		transaction.before);
}
