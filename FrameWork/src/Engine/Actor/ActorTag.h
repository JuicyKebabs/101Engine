#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <string_view>
#include <algorithm>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Tag system
// FNV-1a hash based tag registry.
// Tags are registered at startup and referenced by hash value (uint32_t).
//
// Usage:
//   TagRegistry::Get().Register("Player");
//   actor->SetTag(TagRegistry::Get().GetId("Player"));
//   if (actor->GetTag() == TagRegistry::Get().GetId("Enemy")) { ... }
// ---------------------------------------------------------------------------

using TagId = uint32_t;
constexpr TagId TAG_NONE = 0;

// Compile-time FNV-1a hash function to calculate tag IDs from string literals
constexpr TagId CalcTagId(std::string_view str)
{
	TagId hash = 2166136261u; // FNV offset basis

	for (char c : str)
	{
		hash ^= static_cast<TagId>(c);
		hash *= 16777619u; // FNV prime
	}

	return hash;
}

class TagRegistry
{
public:
	static TagRegistry& Get();

	// Register a tag name and get its corresponding TagId.
	TagId Register(std::string_view tagName);
	bool RegisterUserTag(std::string_view tagName, TagId* outId = nullptr, std::string* outError = nullptr);
	bool UnregisterUserTag(std::string_view tagName);
	bool ContainsName(std::string_view tagName) const;
	bool IsReserved(TagId id) const;
	static bool NormalizeUserTagName(std::string_view input, std::string& output, std::string* outError = nullptr);
	static bool NamesEqualCaseInsensitive(std::string_view left, std::string_view right);
	static bool ValidateUserTagSet(
		const std::vector<std::string>& input,
		std::vector<std::string>& normalized,
		std::string* outError = nullptr);

	// Get the TagId for a given tag name. (Auto-registers if not found)
	TagId GetId(std::string_view tagName);

	// Get the tag name for a given TagId.
	std::string GetName(TagId id) const;

	// Returns a stable presentation snapshot without exposing the registry storage.
	std::vector<std::pair<TagId, std::string>> GetRegisteredTags() const;

private:
	TagRegistry() = default;
	std::unordered_map<TagId, std::string> m_tags;
};

// Helper macro to get TagId from a string literal tag name.
// NOTE: This must be defined BEFORE the ActorTags namespace below,
//       otherwise "TAG": identifier not found errors occur.
#define TAG(name) TagRegistry::Get().GetId(name)

// ---------------------------------------------------------------------------
// Engine-reserved tags
// ---------------------------------------------------------------------------
namespace ActorTags
{
	inline const TagId None       = TAG_NONE;
	inline const TagId MainCamera = TAG("MainCamera");
	inline const TagId InitialSky = TAG("InitialSky");
}
