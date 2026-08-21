#pragma once
#include <string_view>
#include "Engine/Core/Math/Math.h"

//--------------------------------------------------------------------------
// Editor UI utilities
// These are helper functions for creating custom UI elements in the editor
//--------------------------------------------------------------------------

// Forward declarations
class AssetManager;
enum class AssetType;
struct Guid;

namespace EditorUI
{
	struct EditResult
	{
		bool changed = false;
		bool activated = false;
		bool deactivatedAfterEdit = false;

		operator bool() const { return changed; }
	};

	// Separate a label and a value with a horizontal layout

	// Begin a property grid with a unique identifier
	bool BeginPropertyGrid(std::string_view id);
	void EndPropertyGrid();

	// Property fields for various data types
	bool BoolField(std::string_view label, bool& value);
	EditResult FloatField(std::string_view label, float& value, float speed = 0.1f);
	EditResult UIntField(std::string_view label, unsigned int& value, float speed = 1.0f);
	EditResult Vector2Field(std::string_view label, Vector2& value, float speed = 0.1f);
	EditResult Vector3Field(std::string_view label, Vector3& value, float speed = 0.1f);
	EditResult ColorField(std::string_view label, Vector4& value);
	bool ComboField(
		std::string_view label,
		int& currentIndex,
		const char* const items[],
		int itemCount
	);

	bool AssetField(
		std::string_view label,
		const AssetManager& assetManager,
		AssetType assetType,
		const Guid& currentAssetId,
		Guid& outSelectedAssetId
	);
}
