#pragma once

#include "Engine/Core/GUID/Guid.h"
#include "Engine/Core/Math/Math.h"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>
#include <cstdint>



struct AnimationClipHandle
{
	static const std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

	std::uint32_t index = InvalidIndex;
	std::uint32_t generation = 0;
};

template<class T>
struct AnimationKey
{
	double timeSeconds = 0;
	T value;
};

struct AnimationTrack
{
	std::uint32_t nodeIndex = 0;
	std::vector<AnimationKey<Vector3>> positions;
	std::vector<AnimationKey<Quaternion>> rotations;
	std::vector<AnimationKey<Vector3>> scales;
};

struct AnimationClipNode
{
	std::string name;
	std::int32_t parentIndex = -1;	// -1 is root node.
};

struct AnimationClip
{
	Guid sourceModelGuid;
	double durationSeconds = 0.0;
	std::vector<AnimationClipNode> nodes;
	std::vector<AnimationTrack> tracks;
};

nlohmann::json SerializeAnimationClip(const AnimationClip& clip);
AnimationClip DeserializeAnimationClip(const nlohmann::json& json);
