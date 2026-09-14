#include "Engine/Audio/Audio.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/AssetManagerAssetReferenceContext.h"
#include "Engine/Resource/AssetReference.h"
#include <filesystem>
#include <fstream>
#include <iostream>

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
			("101AudioAssets-" + GuidGenerator::Generate().ToString());

		Fixture()
		{
			fs::create_directories(root / "Player");
			fs::create_directories(root / "Enemy");
		}

		~Fixture()
		{
			const fs::path parent = fs::absolute(root).lexically_normal().parent_path();
			if (parent == fs::absolute(fs::temp_directory_path()).lexically_normal())
				fs::remove_all(root);
		}

		void Add(const fs::path& relativePath)
		{
			std::ofstream(root / relativePath, std::ios::binary).put('\0');
		}

		void AddValidWav(const fs::path& relativePath)
		{
			std::ofstream file(root / relativePath, std::ios::binary);
			auto Write16 = [&](uint16_t value) { file.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
			auto Write32 = [&](uint32_t value) { file.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
			file.write("RIFF", 4); Write32(38); file.write("WAVE", 4);
			file.write("fmt ", 4); Write32(16); Write16(1); Write16(1);
			Write32(8000); Write32(8000); Write16(1); Write16(8);
			file.write("data", 4); Write32(2);
			const char silence[2] = { static_cast<char>(128), static_cast<char>(128) };
			file.write(silence, sizeof(silence));
		}
	};

	void TestCatalogPilot()
	{
		static_assert(AssetReference<AudioAsset>::GetExpectedType() == AssetType::Audio);
		Fixture fixture;
		fixture.Add("Player/hit.wav");
		fixture.Add("Enemy/hit.WAV");

		AssetManager assets;
		Check(assets.Initialize(fixture.root.string(), nullptr, nullptr),
			"Audio pilot: catalog initializes without a runtime AudioManager");
		const AssetEntry* player = assets.GetAssetEntryByPath("Player/hit.wav");
		const AssetEntry* enemy = assets.GetAssetEntryByPath("Enemy/hit.WAV");
		Check(player && enemy && player->type == AssetType::Audio && enemy->type == AssetType::Audio,
			"WAV discovery is case-insensitive and distinguishes equal filenames by relative path");
		if (!player || !enemy) return;

		Check(player->guid != enemy->guid && assets.GetAssetEntries(AssetType::Audio).size() == 2,
			"Each discovered Audio asset has a distinct catalog identity");
		AssetManagerAssetReferenceContext context(assets);
		Check(context.Resolve(player->guid, AssetType::Audio) == AssetReferenceCodecResult::Success,
			"Typed Audio references resolve through the existing catalog boundary");
		Check(context.Resolve(player->guid, AssetType::Texture) == AssetReferenceCodecResult::AssetTypeMismatch,
			"Audio references reject a different expected asset type");
		Check(assets.GetAudioHandle(player->guid) == InvalidAudioHandle &&
			assets.GetAudioHandleByPath("Player/../Player/hit.wav") == InvalidAudioHandle,
			"Catalog lookup remains safe when no runtime AudioManager is connected");

		const Guid playerGuid = player->guid;
		AssetManager restarted;
		Check(restarted.Initialize(fixture.root.string(), nullptr, nullptr) &&
			restarted.GetAssetEntryByPath("Player/hit.wav") &&
			restarted.GetAssetEntryByPath("Player/hit.wav")->guid == playerGuid,
			"Audio metadata preserves GUID identity across AssetManager instances");
	}

	void TestInvalidRuntimeHandles()
	{
		AudioManager& audio = AudioManager::GetInstance();
		audio.Terminate();
		Check(!audio.IsLoaded(InvalidAudioHandle) &&
			!audio.Play(InvalidAudioHandle) &&
			!audio.Stop(InvalidAudioHandle) &&
			!audio.Unload(InvalidAudioHandle),
			"Invalid AudioHandle operations fail without touching XAudio2");
		Check(audio.Load("missing.wav") == InvalidAudioHandle,
			"Loading without an initialized runtime fails safely");
	}

	void TestRuntimePilot()
	{
		AudioManager& audio = AudioManager::GetInstance();
		if (!audio.Initialize())
		{
			std::cout << "[SKIP] XAudio2 runtime is unavailable; catalog behavior remains covered.\n";
			return;
		}

		Fixture fixture;
		fixture.AddValidWav("Player/silence.wav");
		AssetManager assets;
		Check(assets.Initialize(fixture.root.string(), nullptr, nullptr, &audio),
			"Runtime pilot: AssetManager connects to an initialized AudioManager");
		const AssetEntry* entry = assets.GetAssetEntryByPath("Player/silence.wav");
		const Guid guid = entry ? entry->guid : Guid{};
		const AudioHandle first = assets.GetAudioHandle(guid);
		const AudioHandle repeated = assets.GetAudioHandleByPath("Player/silence.wav");
		Check(first != InvalidAudioHandle && repeated == first && audio.IsLoaded(first),
			"GUID and relative-path lookup share one lazily loaded AudioHandle");
		Check(audio.Play(first) && audio.Stop(first) && audio.Unload(first),
			"Valid AudioHandle operations enter the existing command pipeline");
		audio.Update();
		Check(!audio.IsLoaded(first), "Unload releases the runtime AudioHandle");
		const AudioHandle reloaded = assets.GetAudioHandle(guid);
		Check(reloaded != InvalidAudioHandle && audio.IsLoaded(reloaded),
			"AssetManager detects an unloaded cached handle and reloads the asset");
		fs::remove(fixture.root / "Player/silence.wav");
		Check(assets.Refresh() && assets.GetAudioHandle(guid) == InvalidAudioHandle,
			"A removed catalog identity cannot resolve through a stale handle cache");
		audio.Terminate();
	}
}

int main()
{
	TestCatalogPilot();
	TestInvalidRuntimeHandles();
	TestRuntimePilot();
	if (g_failures == 0)
	{
		std::cout << "All Audio asset tests passed.\n";
		return 0;
	}
	std::cerr << g_failures << " Audio asset test(s) failed.\n";
	return 1;
}
