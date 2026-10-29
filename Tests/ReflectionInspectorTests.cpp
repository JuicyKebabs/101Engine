#include "imgui.h"
#include <stdexcept>
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
		Check(!inspector.HasActiveEdit(), "Inspector starts without an active transaction");
		Check(inspector.BeginEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 1 }) &&
			inspector.HasActiveEdit(),
			"Transaction begins with the original value");
		Check(inspector.PreviewEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 5 }) && object.value == 5,
			"Preview writes through PropertyMetadata");
		Check(inspector.CommitEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 5 }, callbacks) &&
			commits == 1 && !inspector.HasActiveEdit(),
			"Commit notifies exactly once");

		Check(inspector.BeginEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 5 }) &&
			inspector.HasActiveEdit() &&
			inspector.PreviewEdit(*metadata, typeid(Object), &object, property, std::int64_t{ 9 }) &&
			inspector.CancelActiveEdit() && !inspector.HasActiveEdit() && object.value == 5,
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

		editedObject = std::make_unique<Object>();
		stableTarget.value = 7;
		callbacks.onEditCommit = [](const auto&, const auto&, const auto&) { return false; };
		const bool commitBegan = inspector.BeginEdit(
			*metadata,
			typeid(Object),
			editedObject.get(),
			property,
			std::int64_t{ 1 },
			callbacks);
		const bool commitPreviewed = inspector.PreviewEdit(
			*metadata,
			typeid(Object),
			editedObject.get(),
			property,
			std::int64_t{ 9 });
		void* destroyedIdentity = editedObject.get();
		editedObject.reset();
		const bool commitRestored = inspector.CommitEdit(
			*metadata,
			typeid(Object),
			destroyedIdentity,
			property,
			std::int64_t{ 9 },
			callbacks);
		Check(commitBegan && commitPreviewed && commitRestored && stableTarget.value == 1,
			"Commit failure restores through the stable callback after the preview object is destroyed");
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

// CPU regression of real ImGui events and ReflectionInspector::Draw. No
// Windows input or private ImGui implementation is used to fake UI success.

namespace
{
struct EditableObject
{
    bool earlier = false;
    Vector3 first;
    bool second = false;
};

void RequireInteraction(bool condition, const char* explanation)
{
    if (!condition)
    {
        throw std::runtime_error(explanation);
    }
}

struct InteractionFixture
{
    EditableObject object;
    std::optional<TypeMetadata> metadata;
    ReflectionInspector inspector;
    ReflectionInspectorCallbacks callbacks;
    ImVec2 firstPoint;
    ImVec2 secondPoint;
    ImVec2 earlierPoint;
    unsigned commits = 0;
    unsigned cancels = 0;

    InteractionFixture()
    {
        object.first.x = 1;
        TypeMetadataBuilder<EditableObject> builder("R10InspectorInteraction");
        builder.Property("earlier", &EditableObject::earlier);
        builder.Property("first", &EditableObject::first);
        builder.Property("second", &EditableObject::second);
        auto built = builder.Build();
        RequireInteraction(built.has_value(), "Metadata build failed");
        metadata = std::move(*built);
        ImGui::CreateContext();
        auto& input = ImGui::GetIO();
        input.IniFilename = nullptr;
        input.DisplaySize.x = 640;
        input.DisplaySize.y = 480;
        input.DeltaTime = 1.0f / 60;
        unsigned char* fontPixels = nullptr;
        int fontWidth = 0;
        int fontHeight = 0;
        input.Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);
        callbacks.onEditCommit = [this](const PropertyMetadata&, const PropertyValue&, const PropertyValue&)
        {
            ++commits;
            return true;
        };
        callbacks.onEditCancel = [this](const PropertyMetadata&, const PropertyValue&, bool restored)
        {
            RequireInteraction(restored, "Canceled edit could not be restored");
            ++cancels;
        };
        Frame();
        // Allow initial Window/navigation focus to settle before mouse input.
        // Otherwise ImGui classifies the first auto-focused item as keyboard
        // activation, which is a different interaction from an ordinary click.
        Frame();
        Frame();
    }

    ~InteractionFixture()
    {
        inspector.CancelActiveEdit();
        ImGui::DestroyContext();
    }

    void Frame()
    {
        ImGui::NewFrame();
        ImVec2 position;
        position.x = 10;
        position.y = 10;
        ImVec2 size;
        size.x = 500;
        size.y = 300;
        ImGui::SetNextWindowPos(position);
        ImGui::SetNextWindowSize(size);
        ImGui::Begin("R10 interaction fixture", nullptr, ImGuiWindowFlags_NoSavedSettings);
        const ImVec2 firstMinimum = ImGui::GetCursorScreenPos();
        earlierPoint.x = firstMinimum.x + 8;
        earlierPoint.y = firstMinimum.y + 8;
        firstPoint.x = firstMinimum.x + 40;
        firstPoint.y = firstMinimum.y + ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight() / 2;
        const bool drawn = inspector.Draw(*metadata, typeid(EditableObject), &object,
            ReflectionInspectorPolicy::Editable, callbacks);
        const ImVec2 secondMinimum = ImGui::GetItemRectMin();
        secondPoint.x = secondMinimum.x + 8;
        secondPoint.y = secondMinimum.y + 8;
        ImGui::End();
        ImGui::Render();
        const auto& input = ImGui::GetIO();
        std::cout << "interaction-frame=" << ImGui::GetFrameCount()
            << " mouse=" << input.MousePos.x << ',' << input.MousePos.y
            << " down=" << input.MouseDown[0] << " active=" << ImGui::IsAnyItemActive()
            << " transaction=" << inspector.HasActiveEdit() << '\n';
        RequireInteraction(drawn, "Real Inspector Draw rejected a valid property interaction");
    }

    void Mouse(ImVec2 point, bool down)
    {
        auto& input = ImGui::GetIO();
        input.AddMousePosEvent(point.x, point.y);
        input.AddMouseButtonEvent(0, down);
        Frame();
    }
};
}

int RunUnchangedNumericInteractionChecks()
{
    try
    {
        InteractionFixture fixture;
        fixture.Mouse(fixture.firstPoint, true);
        RequireInteraction(fixture.inspector.HasActiveEdit(), "Numeric mouse down did not begin the edit");
        fixture.Mouse(fixture.firstPoint, false);
        // DragFloat may enter temporary text mode on an unchanged click.
        // Explicitly move focus away before requiring the transaction to end.
        ImVec2 outside;
        outside.x = fixture.firstPoint.x;
        outside.y = fixture.secondPoint.y + 100;
        fixture.Mouse(outside, true);
        fixture.Mouse(outside, false);
        RequireInteraction(!fixture.inspector.HasActiveEdit(), "Unchanged numeric click left a live edit transaction");
        RequireInteraction(fixture.commits == 0, "An unchanged click created an Undo entry");
        fixture.Mouse(fixture.secondPoint, true);
        fixture.Mouse(fixture.secondPoint, false);
        RequireInteraction(fixture.object.second && fixture.commits == 1,
            "Subsequent Bool edit was blocked after an unchanged numeric click");
        RequireInteraction(!fixture.inspector.HasActiveEdit(), "Discrete edit retained a transaction");

        fixture.Mouse(fixture.firstPoint, true);
        ImVec2 moved = fixture.firstPoint;
        moved.x += 40;
        fixture.Mouse(moved, true);
        fixture.Mouse(moved, false);
        RequireInteraction(fixture.object.first.x != 1 && fixture.commits == 2,
            "A real changed numeric drag did not commit exactly once");
        RequireInteraction(!fixture.inspector.HasActiveEdit(), "Changed drag did not close its transaction");
        fixture.Mouse(fixture.firstPoint, true);
        fixture.Mouse(fixture.firstPoint, false);
        fixture.Mouse(fixture.earlierPoint, true);
        fixture.Mouse(fixture.earlierPoint, false);
        RequireInteraction(fixture.object.earlier && fixture.commits == 3,
            "Switching from an unchanged numeric field to an earlier row was blocked");
        std::cout << "PASS unchanged numeric click ends without history; next Bool and changed drag commit once\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

int main()
{
    Check(RunUnchangedNumericInteractionChecks() == EXIT_SUCCESS,
        "Real Draw closes unchanged numeric interactions");
	TestColorPresentation();
	TestTransformPilot();
	TestRowsAndTransaction();
	TestDispatch();
	TestStableRestoreDoesNotUseDestroyedObject();
	return failures == 0 ? 0 : 1;
}
