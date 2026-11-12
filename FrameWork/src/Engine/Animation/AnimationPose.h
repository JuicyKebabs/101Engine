#pragma once

#include "Engine/Resource/AnimationClip.h"
#include "RendererAssets/AssetFiles.h"

#include <span>
#include <vector>

namespace animation
{

struct TrackBinding
{
	std::size_t targetNodeIndex;
	Transform3D defaultLocalTransform;
};

// Binds the animation clip tracks to the model nodes based on their names and hierarchy.
// Decomposes the model node's local transform into default values for any missing track data.
bool BindTracks(
	const AnimationClip& clip,
	std::span<const rendering::assets::ModelNode> modelNodes,
	std::vector<TrackBinding>& output);

bool EvaluatePose(
	const AnimationClip& clip,
	std::span<const TrackBinding> bindings,
	std::span<const rendering::assets::ModelNode> modelNodes,
	double timeSeconds,
	std::vector<Matrix4x4>& output);

}