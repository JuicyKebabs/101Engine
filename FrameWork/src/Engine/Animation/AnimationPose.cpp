#include "AnimationPose.h"
#include "SkinningPalette.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

namespace
{

using ModelNode = rendering::assets::ModelNode;

bool SameNodePath(
	std::span<const AnimationClipNode> clipNodes,
	std::size_t clipIndex,
	std::span<const ModelNode> modelNodes,
	std::size_t modelIndex)
{
	// Walk up the hierarchy of both the clip and model nodes,
	// comparing names and parent indices.
	while (clipIndex < clipNodes.size() && modelIndex < modelNodes.size())
	{
		const auto& clipNode = clipNodes[clipIndex];
		const auto& modelNode = modelNodes[modelIndex];

		// Compare the names of the nodes.
		if (clipNode.name != modelNode.name)
		{
			return false;
		}

		// If either node is a root, they must both be roots to match.
		if (clipNode.parentIndex == -1 || modelNode.parentIndex == -1)
		{
			return clipNode.parentIndex == modelNode.parentIndex;
		}

		// If the parent indices are out of bounds, the nodes cannot match.
		if (clipNode.parentIndex < 0 || modelNode.parentIndex < 0)
		{
			return false;
		}

		// Move up to the parent nodes for the next iteration.
		const auto clipParent = static_cast<std::size_t>(clipNode.parentIndex);
		const auto modelParent = static_cast<std::size_t>(modelNode.parentIndex);

		// If the parent indices are not strictly less than the current indices,
		// the hierarchy is invalid or the nodes do not match.
		if (clipParent >= clipIndex || modelParent >= modelIndex)
		{
			return false;
		}

		clipIndex = clipParent;
		modelIndex = modelParent;
	}

	return false;
}

template<class T>
T Sample(const std::vector<AnimationKey<T>>& keys, double time, const T& fallback)
{
	if (keys.empty())
	{
		return fallback;
	}

	// Use upper_bound to find the first key with a time greater than the specified time.
	const auto upper = std::upper_bound(
		keys.begin(), keys.end(), time,
		[](double t, const AnimationKey<T>& key)
		{
			return t < key.timeSeconds;
		});

	// If the time is before the first key, return the first key's value.
	if (upper == keys.begin())
	{
		return upper->value;
	}

	// If the time is after the last key, return the last key's value.
	const auto& lower = *std::prev(upper);

	if (upper == keys.end())
	{
		return lower.value;
	}

	// Calculate the interpolation factor between the lower and upper keys.
	const double span = upper->timeSeconds - lower.timeSeconds;
	const float factor = static_cast<float>(
		std::isfinite(span) ? (time - lower.timeSeconds) / span
		: (time * 0.5 - lower.timeSeconds * 0.5) / (upper->timeSeconds * 0.5 - lower.timeSeconds * 0.5));

	// Interpolate between the lower and upper key values based on the factor.
	if constexpr (std::is_same_v<T, Quaternion>)
	{
		return Quaternion::Slerp(lower.value, upper->value, factor);
	}
	else
	{
		return T::Lerp(lower.value, upper->value, factor);
	}
}

}

namespace animation
{

bool BindTracks(
	const AnimationClip& clip,
	std::span<const ModelNode> modelNodes,
	std::vector<TrackBinding>& output)
{
	std::vector<TrackBinding> bindings;
	bindings.reserve(clip.tracks.size());
	std::vector<bool> mapped(modelNodes.size());

	// For each track in the animation clip, find the corresponding model node by comparing names and hierarchy.
	// If a match is found, create a TrackBinding and store it in the output vector.
	for (const auto& track : clip.tracks)
	{
		std::optional<std::size_t> match;

		// Search for a matching model node for the current track.
		for (std::size_t index = 0; index < modelNodes.size(); ++index)
		{
			if (!SameNodePath(clip.nodes, track.nodeIndex, modelNodes, index))
			{
				continue;
			}

			// If a match has already been found,
			// the track is ambiguous and cannot be bound.
			if (match)
			{
				return false;
			}

			match = index;	// Store the index of the matching model node.
		}

		if (!match || mapped[*match])
		{
			return false;
		}

		mapped[*match] = true;	// Mark the model node as mapped.

		// Create a TrackBinding for the matched model node
		// and store the default local transform if needed.
		TrackBinding binding;
		binding.targetNodeIndex = *match;

		const bool hasKeys = !track.positions.empty() || !track.rotations.empty() || !track.scales.empty();
		const bool needsDefaults = track.positions.empty() || track.rotations.empty() || track.scales.empty();
		auto& defaults = binding.defaultLocalTransform;

		// If the track has keys and needs default values, decompose the model node's local transform.
		if (hasKeys &&
			needsDefaults &&
			!modelNodes[*match].localTransform.Decompose(defaults.position, defaults.rotation, defaults.scale))
		{
			return false;
		}

		bindings.push_back(binding);
	}

	output = std::move(bindings);
	return true;
}

bool EvaluatePose(
	const AnimationClip& clip,
	std::span<const TrackBinding> bindings,
	std::span<const ModelNode> modelNodes,
	double timeSeconds,
	std::vector<Matrix4x4>& output)
{
	if (bindings.size() != clip.tracks.size() || !std::isfinite(timeSeconds))
	{
		return false;
	}

	std::vector<Matrix4x4> localPose;
	localPose.reserve(modelNodes.size());

	for (const auto& node : modelNodes)
	{
		localPose.push_back(node.localTransform);
	}

	// For each binding, sample the animation track at the specified time
	// and update the local pose of the corresponding model node.
	for (std::size_t index = 0; index < bindings.size(); ++index)
	{
		const auto& binding = bindings[index];
		const auto& track = clip.tracks[index];

		if (binding.targetNodeIndex >= localPose.size())
		{
			return false;
		}

		if (track.positions.empty() && track.rotations.empty() && track.scales.empty())
		{
			continue;
		}

		const auto& defaults = binding.defaultLocalTransform;
		
		// Sample the position, rotation, and scale from the animation track at the specified time.
		Transform3D localTransform;
		localTransform.position = Sample(track.positions, timeSeconds, defaults.position);
		localTransform.rotation = Sample(track.rotations, timeSeconds, defaults.rotation);
		localTransform.scale = Sample(track.scales, timeSeconds, defaults.scale);

		// Update the local pose of the model node with the sampled transform.
		localPose[binding.targetNodeIndex] = localTransform.GetMatrix();
	}

	return skinning::BuildModelPose(modelNodes, localPose, output);
}

}