#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <string_view>

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
	static TagRegistry& Get()
	{
		static TagRegistry instance;
		return instance;
	}

	// Register a tag name and get its corresponding TagId.
	TagId Register(std::string_view tagName)
	{
		TagId id = CalcTagId(tagName);
		m_tags[id] = std::string(tagName);
		return id;
	}

	// Get the TagId for a given tag name. (Auto-registers if not found)
	TagId GetId(std::string_view tagName)
	{
		TagId id = CalcTagId(tagName);
		if (m_tags.find(id) == m_tags.end())
		{
			m_tags[id] = std::string(tagName);
		}
		return id;
	}

	// Get the tag name for a given TagId.
	std::string GetName(TagId id) const
	{
		if (id == TAG_NONE) return "None";
		auto it = m_tags.find(id);
		if (it != m_tags.end()) return it->second;
		return "UnknownTag";
	}

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
}
