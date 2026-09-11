#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Core/Reflection/AssetReferenceCodec.h"
#include "Engine/Core/Reflection/PropertyMetadata.h"
#include "Engine/Core/Reflection/ReflectionSerialization.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Resource/AssetReference.h"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
	struct AssetObject
	{
		AssetReference<TextureAsset> texture;
	};

	int g_failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (condition)
		{
			std::cout << "[PASS] " << name << '\n';
			return;
		}

		std::cerr << "[FAIL] " << name << '\n';
		++g_failures;
	}

	class TestAssetCatalog
		: public AssetReferenceSaveContext,
		  public AssetReferenceRestoreContext
	{
	public:
		Guid guid;
		AssetType type = AssetType::Unknown;

		AssetReferenceCodecResult Validate(
			const Guid& candidate,
			AssetType expectedType) const override
		{
			return CheckReference(candidate, expectedType);
		}

		AssetReferenceCodecResult Resolve(
			const Guid& candidate,
			AssetType expectedType) const override
		{
			return CheckReference(candidate, expectedType);
		}

	private:
		AssetReferenceCodecResult CheckReference(
			const Guid& candidate,
			AssetType expectedType) const
		{
			if (candidate != guid) return AssetReferenceCodecResult::AssetNotFound;
			if (expectedType != type) return AssetReferenceCodecResult::AssetTypeMismatch;
			return AssetReferenceCodecResult::Success;
		}
	};

	TypeMetadata BuildMetadata()
	{
		return *TypeMetadataBuilder<AssetObject>("AssetObject")
			.AddMember("texture", &AssetObject::texture)
			.Build();
	}

	void TestReferenceStatesAndTypeMetadata()
	{
		static_assert(AssetReference<MeshAsset>::GetExpectedType() == AssetType::Mesh);
		static_assert(AssetReference<TextureAsset>::GetExpectedType() == AssetType::Texture);

		AssetReference<TextureAsset> reference;
		Check(!reference.HasValue() && !reference.IsResolved(),
			"Default AssetReference is empty");

		const Guid guid = GuidGenerator::Generate();
		Check(reference.SetGuid(guid) && reference.HasValue() && !reference.IsResolved(),
			"SetGuid creates a GUID-held unresolved state");
		Check(!reference.SetGuid(Guid{}) && reference.GetGuid() == guid,
			"Invalid Guid does not replace an AssetReference");

		const TypeMetadata metadata = BuildMetadata();
		const PropertyMetadata* property = metadata.FindProperty("texture");
		Check(property && property->GetLogicalType() == PropertyLogicalType::AssetReference &&
			property->GetAssetType() == AssetType::Texture,
			"PropertyMetadata retains the expected asset type");
	}

	void TestNullAndGuidRoundTrip()
	{
		AssetReferenceCodec codec;
		TestAssetCatalog catalog;
		catalog.guid = GuidGenerator::Generate();
		catalog.type = AssetType::Texture;
		const TypeMetadata metadata = BuildMetadata();

		ReflectionSaveContext saveContext;
		saveContext.assetReferenceCodec = &codec;
		saveContext.assetReferenceContext = &catalog;
		ReflectionRestoreContext restoreContext;
		restoreContext.assetReferenceCodec = &codec;
		restoreContext.assetReferenceContext = &catalog;

		AssetObject empty;
		nlohmann::json emptyJson;
		Check(ReflectionSerializer::Serialize(
			metadata, typeid(AssetObject), &empty, emptyJson, saveContext) &&
			emptyJson["texture"].is_null(),
			"Empty AssetReference serializes to null");

		AssetObject restoredEmpty;
		Check(ReflectionDeserializer::Deserialize(
			metadata, typeid(AssetObject), emptyJson, &restoredEmpty, restoreContext) &&
			!restoredEmpty.texture.HasValue(),
			"Null restores an empty typed AssetReference");

		AssetObject source;
		source.texture.SetGuid(catalog.guid);
		nlohmann::json json;
		Check(ReflectionSerializer::Serialize(
			metadata, typeid(AssetObject), &source, json, saveContext) &&
			json["texture"] == catalog.guid.ToString(),
			"AssetReference serializes deterministically as only its Asset GUID");

		AssetObject restored;
		Check(ReflectionDeserializer::Deserialize(
			metadata, typeid(AssetObject), json, &restored, restoreContext) &&
			restored.texture.GetGuid() == catalog.guid &&
			!restored.texture.IsResolved(),
			"Deserialize preserves a typed unresolved Asset GUID");

		AssetReferenceValue value = restored.texture.ToValue();
		Check(codec.Resolve(value, catalog) == AssetReferenceCodecResult::Success &&
			value.IsResolved() &&
			restored.texture.SetValue(value) &&
			restored.texture.IsResolved(),
			"Catalog resolution marks the typed reference as resolved");
	}

	void TestFailureResults()
	{
		AssetReferenceCodec codec;
		TestAssetCatalog catalog;
		catalog.guid = GuidGenerator::Generate();
		catalog.type = AssetType::Mesh;
		AssetReferenceValue value;
		nlohmann::json output;

		Check(codec.Deserialize(42, AssetType::Texture, value) ==
			AssetReferenceCodecResult::InvalidJsonType,
			"Non-string AssetReference JSON reports InvalidJsonType");
		Check(codec.Deserialize("not-a-guid", AssetType::Texture, value) ==
			AssetReferenceCodecResult::InvalidGuid,
			"Malformed Asset GUID reports InvalidGuid");

		const Guid missingGuid = GuidGenerator::Generate();
		Check(codec.Deserialize(missingGuid.ToString(), AssetType::Texture, value) ==
			AssetReferenceCodecResult::Success && !value.IsResolved(),
			"A valid unregistered GUID can be restored unresolved");
		Check(codec.Resolve(value, catalog) == AssetReferenceCodecResult::AssetNotFound,
			"An unregistered Asset reports AssetNotFound during resolution");

		Check(codec.Deserialize(catalog.guid.ToString(), AssetType::Texture, value) ==
			AssetReferenceCodecResult::Success &&
			codec.Resolve(value, catalog) == AssetReferenceCodecResult::AssetTypeMismatch &&
			codec.Serialize(value, catalog, output) == AssetReferenceCodecResult::AssetTypeMismatch,
			"A registered Asset of another type reports AssetTypeMismatch");
	}

	void TestAssetManagerResolutionBoundary()
	{
		namespace fs = std::filesystem;
		const fs::path directory = fs::temp_directory_path() /
			("101EngineAssetReference-" + GuidGenerator::Generate().ToString());
		fs::create_directories(directory);
		std::ofstream(directory / "texture.png").put('\0');
		std::ofstream(directory / "mesh.obj").put('\0');

		AssetManager manager;
		manager.Initialize(directory.string(), nullptr, nullptr);
		AssetManagerAssetReferenceContext context(manager);
		const AssetEntry* texture = manager.GetAssetEntryByPath("texture.png");
		const AssetEntry* mesh = manager.GetAssetEntryByPath("mesh.obj");

		Check(texture && context.Resolve(texture->guid, AssetType::Texture) ==
			AssetReferenceCodecResult::Success,
			"AssetManager context resolves a registered Asset of the expected type");
		Check(mesh && context.Resolve(mesh->guid, AssetType::Texture) ==
			AssetReferenceCodecResult::AssetTypeMismatch,
			"AssetManager context rejects a registered Asset of another type");
		Check(context.Resolve(GuidGenerator::Generate(), AssetType::Texture) ==
			AssetReferenceCodecResult::AssetNotFound,
			"AssetManager context distinguishes an unregistered Asset");

		fs::remove_all(directory);
	}
}

int main()
{
	TestReferenceStatesAndTypeMetadata();
	TestNullAndGuidRoundTrip();
	TestFailureResults();
	TestAssetManagerResolutionBoundary();

	if (g_failures == 0)
	{
		std::cout << "All AssetReferenceCodec tests passed.\n";
		return 0;
	}

	std::cerr << g_failures << " AssetReferenceCodec test(s) failed.\n";
	return 1;
}
