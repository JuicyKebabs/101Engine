#pragma once

#include "Engine/Resource/AnimationClip.h"
#include "RendererAssets/AssetFiles.h"

#include <optional>
#include <span>
#include <vector>


namespace animation
{

using LocalPose = std::vector<std::optional<Transform3D>>;

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

bool EvaluateLocalPose(
	const AnimationClip & clip,
	std::span<const TrackBinding> bindings,
	std::span<const rendering::assets::ModelNode> modelNodes,
	double timeSeconds,
	LocalPose & output);

bool BlendLocalPoses(
	std::span<const rendering::assets::ModelNode> modelNodes,
	const LocalPose & source,
	const LocalPose & destination,
	float weight,
	LocalPose & output);

bool BuildModelPoseFromLocal(
	std::span<const rendering::assets::ModelNode> modelNodes,
	const LocalPose & pose,
	std::vector<Matrix4x4>&output);

bool EvaluatePose(
	const AnimationClip& clip,
	std::span<const TrackBinding> bindings,
	std::span<const rendering::assets::ModelNode> modelNodes,
	double timeSeconds,
	std::vector<Matrix4x4>& output);

}