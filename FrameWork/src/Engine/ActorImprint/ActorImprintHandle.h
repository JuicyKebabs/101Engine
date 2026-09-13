#pragma once
#include <cstdint>

// Runtime identity scoped to one ActorImprintSystem. Never serialized.
struct ActorImprintHandle
{
	std::uint32_t index = UINT32_MAX;
	std::uint32_t generation = 0;

	bool IsNull() const { return index == UINT32_MAX; }
	static ActorImprintHandle Null() { return {}; }
	friend bool operator==(const ActorImprintHandle&, const ActorImprintHandle&) = default;
};
