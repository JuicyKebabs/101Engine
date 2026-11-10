#pragma once

#include "Engine/Core/Math/Math.h"
#include "Engine/Core/GUID/Guid.h"
#include "Renderer/Math.h"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>
#include <cstdint>

template<class T>
struct AnimationKey
{
	double timeSeconds = 0;
	T value;
};

struct AnimationTrack
{
	std::uint32_t nodeIndex = 0;
	std::vector<AnimationKey<rendering::Vector3>> positions;
	std::vector<AnimationKey<rendering::Quaternion>> rotations;
	std::vector<AnimationKey<rendering::Vector3>> scales;
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
