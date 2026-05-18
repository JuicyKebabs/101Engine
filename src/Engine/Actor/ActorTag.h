#pragma once
#include <cstdint>
#include <string_view>

// ---------------------------------------------------------------------------
// Actor tag system
//
// Current state: simple enum (game-specific values are a known issue).
// Planned migration: replace with string-hash tags registered at startup,
// so game code can extend tags without modifying engine files.
//
// Usage:
//   actor->SetTag(ActorTag::Player);
//   if (actor->GetTag() == ActorTag::Enemy) { ... }
// ---------------------------------------------------------------------------

// TODO: Phase 0.5 - Replace this enum with a TagId (uint32_t FNV-1a hash)
// registered via TagRegistry::Register("Player"), TagRegistry::Register("Enemy"), etc.
// Game-specific tags will then live in game code only.
enum class ActorTag : uint32_t
{
	NONE         = 0,
	PLAYER,
	ENEMY,
	WALL,
	PLAYER_BULLET,
	ENEMY_BULLET,

	// --- Add game-specific tags below this line (temporary; remove on migration) ---

	Max
};

// Keep the old name as an alias so existing code compiles without change.
// Remove after all call sites are updated to ActorTag.
using ACTOR_TAG = ActorTag;
