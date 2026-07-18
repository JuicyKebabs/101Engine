#pragma once
#include <cstdint>
#include <functional>

//-----------------------------------------------------------------------------------------
// ActorHandle struct
// A handle to uniquely identify an actor in the scene.
// Generation is used to avoid dangling references when actors are destroyed and recreated.
// This handle is not persistant across running process.
//-----------------------------------------------------------------------------------------

struct ActorHandle
{
	uint32_t index = UINT32_MAX;
	uint32_t generation = 0;

	bool operator==(const ActorHandle& other) const
	{
		return index == other.index && generation == other.generation;
	}
	bool operator!=(const ActorHandle& other) const
	{
		return !(*this == other);
	}

	bool IsNull() const { return index == UINT32_MAX; }

	static ActorHandle Null() { return ActorHandle{ UINT32_MAX, 0 }; }
};

namespace std
{
	template<>
	struct hash<ActorHandle>
	{
		std::size_t operator()(const ActorHandle& handle) const noexcept
		{
			return (static_cast<size_t>(handle.index) << 32) | handle.generation;
		}
	};
}
