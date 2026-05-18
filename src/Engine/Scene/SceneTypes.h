#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Scene type identifier
//
// Current state: hardcoded enum.
// Planned migration (Phase 0.5): scenes will be registered by name at startup
// from a scene manifest (JSON), so adding scenes requires no engine edits.
//
// Example future API:
//   SceneManager::RegisterScene("Title",  []{ return std::make_unique<TitleScene>(); });
//   SceneManager::RegisterScene("Game",   []{ return std::make_unique<GameScene>(); });
//   sceneManager.ReserveChangeScene("Game");
// ---------------------------------------------------------------------------

// TODO: Phase 0.5 - Replace with string-based scene IDs + factory registration.
enum class SceneType : uint32_t
{
	SCENE_NONE   = 0,
	SCENE_TITLE,
	SCENE_GAME,
	SCENE_RESULT,

	// --- Add scenes below (temporary; remove on migration) ---
};

// Alias for backward compatibility during migration
using SCENE_TYPE = SceneType;
