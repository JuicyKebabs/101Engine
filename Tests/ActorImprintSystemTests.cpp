#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Resource/MetaFile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

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
		fs::path root = fs::temp_directory_path() / ("101ImprintSystem-" + GuidGenerator::Generate().ToString());
		Fixture() { fs::create_directories(root); }
		~Fixture()
		{
			// Only remove the unique test directory directly below the system temp root.
			if (fs::absolute(root).lexically_normal().parent_path() == fs::absolute(fs::temp_directory_path()).lexically_normal())
				fs::remove_all(root);
		}
		Guid Add(const std::string& path)
		{
			fs::copy_file("Tests/Fixtures/ActorImprint/Minimal.imprint", root / path);
			const Guid id = GuidGenerator::Generate();
			Check(MetaFile::Save((root / path).string(), id), "Fixture metadata saves successfully");
			return id;
		}
	};
	void TestPilot()
	{
		static_assert(AssetReference<ActorImprint>::GetExpectedType() == AssetType::ActorImprint);
		Fixture fixture;
		const Guid id = fixture.Add("pilot.imprint");
		AssetManager assets;
		Check(assets.Initialize(fixture.root.string(), nullptr, nullptr), "Pilot: catalog initializes");
		Check(assets.GetAssetEntry(id) &&
			assets.GetAssetEntry(id)->type == AssetType::ActorImprint,
			"Pilot: existing catalog discovers .imprint using .meta identity");
		ActorImprintSystem system(assets);
		Check(system.GetLoadedCount() == 0, "Pilot: catalog discovery does not load definitions");
		AssetReference<ActorImprint> reference; reference.SetGuid(id);

		const auto handle = system.Load(reference);
		const auto* definition = system.Resolve(handle);
		Check(definition && system.GetAssetGuid(handle) == id, "Pilot: typed reference loads a complete ET-08 definition");
		Check(system.Load(id) == handle &&
			system.Resolve(handle) == definition &&
			system.GetLoadedCount() == 1,
			"Pilot: repeated load keeps the same handle and definition");
		Check(system.Unload(handle) && !system.Resolve(handle), "Pilot: unload invalidates old handle");
		const auto reused = system.Load(id);
		Check(reused.index == handle.index &&
			reused.generation != handle.generation &&
			system.Resolve(reused) &&
			!system.Resolve(handle),
			"Pilot: slot reuse cannot resolve a stale handle");
	}

	bool HasChange(const std::vector<AssetChange>& changes, AssetChangeKind kind, AssetType type, const Guid& guid, const std::string& path)
	{
		return std::find(changes.begin(), changes.end(), AssetChange{ kind, type, guid, path }) != changes.end();
	}

	void TestCatalogAndNotifications()
	{
		Fixture fixture;
		const auto imprint = fixture.Add("a.IMPRINT");
		std::ofstream(fixture.root / "texture.png").put('x');
		AssetManager assets;
		Check(assets.Initialize(fixture.root.string(), nullptr, nullptr), "Catalog discovers mixed asset types");
		const auto* texture = assets.GetAssetEntryByPath("texture.png");
		Check(texture && texture->type == AssetType::Texture, "Existing texture discovery still works");
		if (!texture) return;
		const auto textureId = texture->guid;
		Check(MetaFile::TryLoad((fixture.root / "texture.png").string()) == textureId,
			"Missing sidecar gets a persistent nonzero identity");
		auto changes = assets.TakePendingChanges();
		Check(changes.size() == 2 &&
			HasChange(changes, AssetChangeKind::Added, AssetType::ActorImprint, imprint, "a.IMPRINT") &&
			HasChange(changes, AssetChangeKind::Added, AssetType::Texture, textureId, "texture.png"),
			"Added notifications are generic and carry GUID/path/type");
		Check(assets.Refresh() && assets.TakePendingChanges().empty(), "Unchanged refresh emits nothing");
		Check(assets.NotifyAssetChanged("a.IMPRINT") && assets.NotifyAssetChanged("a.IMPRINT"), "Explicit changes do not depend on timestamps");
		changes = assets.TakePendingChanges();
		Check(changes.size() == 1 &&
			HasChange(changes, AssetChangeKind::Modified, AssetType::ActorImprint, imprint, "a.IMPRINT"),
			"Repeated notification coalesces in the pending queue");
		{ std::ofstream file(fixture.root / "texture.png", std::ios::app); file << "changed size"; }
		Check(assets.Refresh(), "External changes are discovered by the existing scanner");
		changes = assets.TakePendingChanges();
		Check(changes.size() == 1 &&
			HasChange(changes, AssetChangeKind::Modified, AssetType::Texture, textureId, "texture.png"),
			"External Modified notifications also work for Texture");
		fs::remove(fixture.root / "a.IMPRINT");
		Check(assets.NotifyAssetChanged("a.IMPRINT") && !assets.GetAssetEntry(imprint), "Explicit removal updates catalog");
		changes = assets.TakePendingChanges();
		Check(changes.size() == 1 &&
			HasChange(changes, AssetChangeKind::Removed, AssetType::ActorImprint, imprint, "a.IMPRINT"),
			"Removed notification retains the old identity and type");
		AssetCatalogError error;
		Check(!assets.NotifyAssetChanged("../outside.imprint", &error) &&
			error.code == AssetCatalogErrorCode::InvalidPath &&
			assets.TakePendingChanges().empty(),
			"Explicit notification rejects paths outside asset root");
	}

	void TestCatalogFailureTransaction()
	{
		Fixture fixture;
		const auto original = fixture.Add("original.imprint");
		Check(!MetaFile::Save((fixture.root / "original.imprint").string(), GuidGenerator::Generate(), MetaFile::WriteMode::CreateNew) &&
			MetaFile::TryLoad((fixture.root / "original.imprint").string()) == original,
			"CreateNew sidecar writes cannot replace an existing persistent identity");
		AssetManager assets;
		Check(assets.Initialize(fixture.root.string(), nullptr, nullptr), "Initial catalog loads before failure injection");
		assets.TakePendingChanges();
		assets.NotifyAssetChanged("original.imprint");
		const auto added = fixture.Add("added.imprint");
		fixture.Add("duplicate.imprint");
		MetaFile::Save((fixture.root / "duplicate.imprint").string(), original);
		AssetCatalogError error;
		Check(!assets.Refresh(&error) &&
			error.code == AssetCatalogErrorCode::DuplicateGuid &&
			!error.path.empty(),
			"Duplicate GUID rejects the entire candidate catalog with a diagnostic");
		Check(assets.GetAssetEntry(original) &&
			!assets.GetAssetEntry(added) &&
			assets.GetAssetEntries(AssetType::ActorImprint).size() == 1,
			"Failed refresh publishes neither partial additions nor overwritten identities");
		const auto preserved = assets.TakePendingChanges();
		Check(preserved.size() == 1 &&
			HasChange(preserved, AssetChangeKind::Modified, AssetType::ActorImprint, original, "original.imprint"),
			"Failed refresh preserves already-pending notifications without publishing candidate events");
		AssetManager cold;
		Check(!cold.Initialize(fixture.root.string(), nullptr, nullptr) &&
			cold.GetAssetEntries(AssetType::ActorImprint).empty() &&
			cold.TakePendingChanges().empty(),
			"Cold initialization also rejects partial duplicate catalogs");
		fs::remove(fixture.root / "duplicate.imprint");
		Check(assets.Refresh() && assets.GetAssetEntry(added), "Valid refresh recovers after duplicate source is removed");
		assets.TakePendingChanges();
		for (const std::string& contents : { "{", "{}", "{\"guid\":42}", "{\"guid\":\"invalid\"}",
			"{\"guid\":\"{00000000-0000-0000-0000-000000000000}\"}" })
		{
			const fs::path meta = fixture.root / "original.imprint.meta";
			{ std::ofstream file(meta); file << contents; }
			Check(!assets.Refresh(&error) &&
				error.code == AssetCatalogErrorCode::InvalidMetadata &&
				assets.GetAssetEntry(original),
				"Invalid existing metadata fails while previous catalog remains usable");
			std::ifstream file(meta);
			const std::string actual((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			Check(actual == contents && assets.TakePendingChanges().empty(), "Invalid sidecar is never overwritten and emits no change");
		}
		Check(!assets.Initialize((fixture.root / "missing").string(), nullptr, nullptr, &error) &&
			assets.GetAssetEntry(original) &&
			assets.GetAssetPath(original) == (fixture.root / "original.imprint").string(),
			"Failed reinitialization preserves root and lookup indices");
	}

	void TestPendingTransitionSequence()
	{
		Fixture fixture;
		const auto imprint = fixture.Add("sequence.imprint");
		AssetManager assets;
		Check(assets.Initialize(fixture.root.string(), nullptr, nullptr), "Transition test catalog initializes");
		assets.TakePendingChanges();

		fs::remove(fixture.root / "sequence.imprint");
		Check(assets.Refresh() && !assets.GetAssetEntry(imprint), "First removal updates the catalog");
		fs::copy_file("Tests/Fixtures/ActorImprint/Minimal.imprint", fixture.root / "sequence.imprint");
		Check(assets.Refresh() && assets.GetAssetEntry(imprint), "Restore keeps the sidecar identity");
		fs::remove(fixture.root / "sequence.imprint");
		Check(assets.Refresh() && !assets.GetAssetEntry(imprint), "Second removal updates the catalog");

		const std::vector<AssetChange> expected{
			{ AssetChangeKind::Removed, AssetType::ActorImprint, imprint, "sequence.imprint" },
			{ AssetChangeKind::Added, AssetType::ActorImprint, imprint, "sequence.imprint" },
			{ AssetChangeKind::Removed, AssetType::ActorImprint, imprint, "sequence.imprint" }
		};
		Check(assets.TakePendingChanges() == expected,
			"Alternating transitions preserve Removed-Added-Removed and the final Removed state");
	}

	void TestPoolFailureAndLifetime()
	{
		Fixture fixture;
		const auto good = fixture.Add("good.imprint");
		const auto bad = fixture.Add("bad.imprint");
		const auto missing = fixture.Add("missing.imprint");
		const auto wrongType = fixture.Add("mesh.obj");
		AssetManager assets;
		assets.Initialize(fixture.root.string(), nullptr, nullptr);
		fs::remove(fixture.root / "missing.imprint");
		{ std::ofstream file(fixture.root / "bad.imprint"); file << "{}"; }
		ActorImprintSystem system(assets);

		const auto valid = system.Load(good);
		const auto* before = system.Resolve(valid);
		Check(system.Load(Guid{}).IsNull(), "Invalid GUID uses the existing AssetReference diagnostic");
		Check(system.Load(GuidGenerator::Generate()).IsNull(), "Unregistered GUID reports missing catalog asset");
		Check(system.Load(wrongType).IsNull(), "Catalog type mismatch never starts Imprint parsing");
		Check(system.Load(missing).IsNull(), "Missing file preserves the ET-08 I/O diagnostic");
		Check(system.Load(bad).IsNull(), "Malformed asset preserves the ET-08 schema location");
		Check(system.GetLoadedCount() == 1 &&
			system.Resolve(valid) == before &&
			system.FindHandle(bad).IsNull(),
			"All failed loads leave the existing pool and GUID map unchanged");
		{ std::ofstream file(fixture.root / "good.imprint"); file << "{}"; }
		Check(system.Load(good) == valid &&
			system.Resolve(valid) == before,
			"Loaded definition cannot be replaced by a subsequent file change before the reload transaction exists");
		Check(!system.Resolve({ valid.index, valid.generation + 1 }) &&
			!system.Unload({ UINT32_MAX, 0 }),
			"Forged generations and null handles do not resolve or unload");
		AssetManagerAssetReferenceContext context(assets);
		AssetReferenceCodec codec;
		AssetReferenceValue value;
		nlohmann::json output;
		Check(codec.Deserialize(good.ToString(), AssetType::ActorImprint, value) == true &&
			codec.Resolve(value, context) == true &&
			codec.Serialize(value, context, output) == true &&
			output == good.ToString(),
			"ActorImprint references reuse the generic AssetReference codec and catalog resolver");
		fs::remove(fixture.root / "good.imprint");
		Check(assets.Refresh() &&
			system.Resolve(valid) == before &&
			system.Load(good).IsNull(),
			"Catalog removal preserves borrowed loaded definition but refuses a fresh load request");
		system.Clear();
		Check(system.GetLoadedCount() == 0 &&
			!system.Resolve(valid) &&
			system.FindHandle(good).IsNull(),
			"Clear invalidates handles before Component module teardown");
		fs::copy_file("Tests/Fixtures/ActorImprint/Minimal.imprint", fixture.root / "good.imprint");
		Check(assets.Refresh(), "Restored file reuses its surviving sidecar identity");
		const auto restored = system.Load(good);
		Check(system.Resolve(restored) && restored != valid && !system.Resolve(valid), "Reload after Clear never revives the old handle");
	}
}

int main()
{
	TestPilot();
	TestCatalogAndNotifications();
	TestCatalogFailureTransaction();
	TestPendingTransitionSequence();
	TestPoolFailureAndLifetime();
	return g_failures ? 1 : 0;
}
