#pragma once

#include "Engine/Core/GUID/Guid.h"

#include <string>
#include <vector>

struct AnimationClipSlot
{
	std::string alias;
	Guid clipAssetId;

	friend bool operator==(const AnimationClipSlot&, const AnimationClipSlot&) = default;
};

struct AnimationClipSlots
{
	std::vector<AnimationClipSlot> slots;
	std::string defaultSlotAlias;	// The alias of the clip used for initial playback.

	friend bool operator==(const AnimationClipSlots&, const AnimationClipSlots&) = default;
};
