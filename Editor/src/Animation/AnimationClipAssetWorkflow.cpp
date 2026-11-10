#include "AnimationClipAssetWorkflow.h"
#include "Engine/Core/Debug/Debug.h"
#include "Core/AssetFileWorkflow.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Resource/BuiltinMeshAssets.h"

#include <exception>

namespace
{
	template<class T>
	bool ConvertKeyValue(
		std::vector<AnimationKey<T>>& outKeys,
		const std::vector<rendering::assets::AnimationKey<T>>& sourceKeys,
		double tickPerSecond)
	{
		outKeys.clear();
		outKeys.reserve(sourceKeys.size());

		for (const auto& source : sourceKeys)
		{
			auto& key = outKeys.emplace_back();
			key.timeSeconds = source.time / tickPerSecond;
			key.value = source.value;
		}

		return true;
	}
}

bool AnimationClipAssetWorkflow::BuildAnimationClip(
	AnimationClip& outClip,
	const Guid& sourceModelGuid,
	const std::vector<rendering::assets::ModelNode>& sourceNodes,
	const rendering::assets::ImportedAnimationClip& importedClips)
{
	if (!std::isfinite(importedClips.ticksPerSecond) || importedClips.ticksPerSecond <= 0)
	{
		DBG("BuildAnimationClip : tickPerSecond has to be finite and greater than zero.");
		return false;
	}

	AnimationClip candidate;

	std::vector<AnimationClipNode>& nodes = candidate.nodes;
	nodes.clear();
	nodes.reserve(sourceNodes.size());

	for (const auto& sourceNode : sourceNodes)
	{
		nodes.push_back({ sourceNode.name, sourceNode.parentIndex });
	}

	auto nodeIndex = [&](const std::string& nodeName) -> std::uint32_t
		{
			const auto missing = std::numeric_limits<std::uint32_t>::max();
			std::uint32_t match = missing;

			for (std::uint32_t i = 0; i < nodes.size(); ++i)
			{
				if (nodes[i].name == nodeName)
				{
					if (match != missing)
					{
						return missing;
					}

					match = i;
				}
			}

			return match;
		};


	std::vector<AnimationTrack>& tracks = candidate.tracks;
	tracks.clear();
	tracks.reserve(importedClips.channels.size());

	for (const auto& channel : importedClips.channels)
	{
		auto& track = tracks.emplace_back();

		std::uint32_t index = nodeIndex(channel.nodeName);

		if (index == std::numeric_limits<uint32_t>::max())
		{
			return false;
		}

		track.nodeIndex = index;

		if (!ConvertKeyValue(track.positions,channel.positions, importedClips.ticksPerSecond) ||
			!ConvertKeyValue(track.rotations, channel.rotations, importedClips.ticksPerSecond) ||
			!ConvertKeyValue(track.scales, channel.scales, importedClips.ticksPerSecond))
		{
			DBG("BuildAnimationClip : Conversion Failed.");
			return false;
		}
	}

	candidate.sourceModelGuid = sourceModelGuid;
	candidate.durationSeconds = importedClips.durationTicks / importedClips.ticksPerSecond;
	outClip = std::move(candidate);

	return true;
}

bool AnimationClipAssetWorkflow::Create(
	AssetManager& assetManager,
	const std::filesystem::path& relativeDirectory,
	std::string_view name,
	const Guid& sourceModelGuid,
	const std::vector<rendering::assets::ModelNode>& sourceNodes,
	const rendering::assets::ImportedAnimationClip& sourceClip,
	Guid& outAssetGuid,
	std::string& diagnostic)
{
	outAssetGuid = {};
	diagnostic.clear();

	if (relativeDirectory.empty() || relativeDirectory.lexically_normal() == ".")
	{
		diagnostic = "Choose a folder below the asset root.";
		return false;
	}

	// Get the entry of the source model asset which is owner of a selected clip.
	const AssetEntry* source = assetManager.GetAssetEntry(sourceModelGuid);

	if (!source || source->type != AssetType::Mesh || FindBuiltinMeshAsset(sourceModelGuid))
	{
		diagnostic = "The source model is no longer available.";
		return false;
	}

	try
	{
		AnimationClip clip;

		if (!BuildAnimationClip(clip, sourceModelGuid, sourceNodes, sourceClip))
		{
			diagnostic = "Could not build the clip. Check the timing and target nodes.";
			return false;
		}

		const std::string bytes = SerializeAnimationClip(clip).dump(2) + '\n';
		DeserializeAnimationClip(nlohmann::json::parse(bytes));

		return AssetFileWorkflow::CreateGenerated(
			assetManager, relativeDirectory, name, L".animclip", bytes,
			outAssetGuid, diagnostic);
	}
	catch (const std::exception& exception)
	{
		diagnostic = exception.what();
		return false;
	}
}
