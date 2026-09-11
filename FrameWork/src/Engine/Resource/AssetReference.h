#pragma once
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetType.h"
#include <type_traits>

// Primary template for converting an asset type to its corresponding AssetType enum value.
template<class Asset>
struct AssetTypeOf
{
	static constexpr AssetType value = AssetType::Unknown;
};

template<>
struct AssetTypeOf<MeshAsset>
{
	static constexpr AssetType value = AssetType::Mesh;
};

template<>
struct AssetTypeOf<TextureAsset>
{
	static constexpr AssetType value = AssetType::Texture;
};

// Type-erased value used while an AssetReference passes through Reflection.
// Recognize thr type by the expectedType field.
// guid is the unique ID of the asset with no type information
struct AssetReferenceValue
{
	Guid guid;
	AssetType expectedType = AssetType::Unknown;
	bool resolved = false;

	bool HasValue() const { return guid.IsValid(); }
	bool IsResolved() const { return HasValue() && resolved; }

	friend bool operator==(const AssetReferenceValue&, const AssetReferenceValue&) = default;
};

// AssetReference is a type-safe reference to an asset of a specific type.
// Receive an AssetType when instantiated, and can be used to store a GUID for that asset type without knowing the actual asset type at runtime.
template<class Asset>
class AssetReference
{
	// Compile-time check to ensure that the Asset type is registered and has a known AssetType.
	static_assert(
		AssetTypeOf<Asset>::value != AssetType::Unknown,
		"AssetReference requires a registered asset type.");

public:
	// Type-erased getter for the expected AssetType of this AssetReference.
	static constexpr AssetType GetExpectedType() { return AssetTypeOf<Asset>::value; }

	bool HasValue() const { return m_guid.IsValid(); }
	bool IsResolved() const { return HasValue() && m_resolved; }
	const Guid& GetGuid() const { return m_guid; }

	bool SetGuid(const Guid& guid)
	{
		if (!guid.IsValid()) return false;
		m_guid = guid;
		m_resolved = false;
		return true;
	}

	void Clear()
	{
		m_guid = {};
		m_resolved = false;
	}

	// Converts this AssetReference to a type-erased AssetReferenceValue for reflection purposes.
	AssetReferenceValue ToValue() const
	{
		return { m_guid, GetExpectedType(), IsResolved() };
	}

	bool SetValue(const AssetReferenceValue& value)
	{
		if (value.expectedType != GetExpectedType()) return false;
		if (!value.HasValue())
		{
			Clear();
			return true;
		}

		m_guid = value.guid;
		m_resolved = value.resolved;
		return true;
	}

	friend bool operator==(const AssetReference&, const AssetReference&) = default;

private:
	Guid m_guid;
	bool m_resolved = false;	// Catalog has the asset with this guid been and same asset type as expected T.
};

// Type trait to check if a given type T is an AssetReference of some asset type.
template<class T>
struct IsAssetReference : std::false_type {};

// Specialization for AssetReference types, which inherits from std::true_type.
template<class Asset>
struct IsAssetReference<AssetReference<Asset>> : std::true_type {};

// Template helpers to easily check if a type is an AssetReference at compile time.
template<class T>
inline constexpr bool IsAssetReferenceV = IsAssetReference<T>::value;