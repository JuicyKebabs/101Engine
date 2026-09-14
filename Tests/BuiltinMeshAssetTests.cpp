#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Resource/BuiltinMeshAssets.h"
#include "Engine/Resource/MetaFile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>

namespace
{
	namespace fs = std::filesystem;
	int g_failures = 0;

	void Check(bool condition, const char* name)
	{
		if (condition) std::cout << "[PASS] " << name << '\n';
		else { ++g_failures; std::cerr << "[FAIL] " << name << '\n'; }
	}

	struct Fixture
	{
		fs::path root = fs::temp_directory_path() /
			("101BuiltinMeshAssets-" + GuidGenerator::Generate().ToString());

		Fixture() { fs::create_directories(root); }
		~Fixture()
		{
			if (fs::absolute(root).lexically_normal().parent_path() ==
				fs::absolute(fs::temp_directory_path()).lexically_normal())
			{
				fs::remove_all(root);
			}
		}
	};

	void TestDefinitions()
	{
		const auto builtins = GetBuiltinMeshAssets();
		Check(builtins.size() == 6, "All default meshes have built-in asset definitions");

		std::unordered_set<Guid> guids;
		std::unordered_set<std::string> paths;
		bool definitionsValid = true;
		bool geometryValid = true;
		for (const BuiltinMeshAsset& builtin : builtins)
		{
			definitionsValid = definitionsValid && builtin.guid.IsValid() &&
				guids.insert(builtin.guid).second && paths.emplace(builtin.path).second &&
				FindBuiltinMeshAsset(builtin.guid) == &builtin;
			geometryValid = geometryValid && !GetDefaultModel(builtin.mesh).empty();
		}
		Check(definitionsValid, "Built-in mesh GUIDs and picker paths are valid and unique");
		Check(geometryValid, "Every built-in definition resolves to procedural geometry");
	}

	void TestCatalogIntegration()
	{
		Fixture fixture;
		{ std::ofstream file(fixture.root / "project.obj"); file << "# catalog-only test"; }

		AssetManager assets;
		Check(assets.Initialize(fixture.root.string(), nullptr, nullptr),
			"Catalog initializes with built-in and file meshes");
		const auto meshes = assets.GetAssetEntries(AssetType::Mesh);
		Check(meshes.size() == 7, "Mesh enumeration contains six built-ins and one project mesh");

		AssetManagerAssetReferenceContext referenceContext(assets);
		bool catalogValid = true;
		for (const BuiltinMeshAsset& builtin : GetBuiltinMeshAssets())
		{
			const AssetEntry* byGuid = assets.GetAssetEntry(builtin.guid);
			const AssetEntry* byPath = assets.GetAssetEntryByPath(std::string(builtin.path));
			catalogValid = catalogValid && byGuid && byPath && byGuid == byPath &&
				byGuid->type == AssetType::Mesh && byGuid->relativePath == builtin.path &&
				assets.GetAssetPath(builtin.guid).empty() &&
				referenceContext.Validate(builtin.guid, AssetType::Mesh) ==
					AssetReferenceCodecResult::Success;
		}
		Check(catalogValid, "Built-in meshes use the normal catalog and asset-reference lookup paths");

		const auto initialChanges = assets.TakePendingChanges();
		Check(initialChanges.size() == 1 && initialChanges.front().relativePath == "project.obj",
			"Built-in meshes do not emit file change notifications");
		Check(assets.Refresh() && assets.TakePendingChanges().empty(),
			"Unchanged refresh does not emit built-in mesh changes");

		AssetManager restarted;
		Check(restarted.Initialize(fixture.root.string(), nullptr, nullptr),
			"A second catalog instance initializes");
		bool identitiesStable = true;
		for (const BuiltinMeshAsset& builtin : GetBuiltinMeshAssets())
		{
			const AssetEntry* entry = restarted.GetAssetEntryByPath(std::string(builtin.path));
			identitiesStable = identitiesStable && entry && entry->guid == builtin.guid;
		}
		Check(identitiesStable, "Built-in mesh identities remain stable across catalog instances");
	}

	void TestBuiltinGuidCollision()
	{
		Fixture fixture;
		const fs::path meshPath = fixture.root / "collision.obj";
		{ std::ofstream file(meshPath); file << "# GUID collision test"; }
		const Guid builtinGuid = GetBuiltinMeshAssets().front().guid;
		Check(MetaFile::Save(meshPath.string(), builtinGuid), "Collision fixture metadata saves");

		AssetManager assets;
		AssetCatalogError error;
		Check(!assets.Initialize(fixture.root.string(), nullptr, nullptr, &error) &&
			error.code == AssetCatalogErrorCode::DuplicateGuid &&
			assets.GetAssetEntries(AssetType::Mesh).empty(),
			"A project asset cannot replace a built-in mesh identity");
	}
}

int main()
{
	TestDefinitions();
	TestCatalogIntegration();
	TestBuiltinGuidCollision();
	return g_failures ? 1 : 0;
}

