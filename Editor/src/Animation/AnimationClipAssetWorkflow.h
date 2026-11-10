#pragma once

#include <filesystem>
#include <string_view>

#include "Engine/Resource/AnimationClip.h"
#include "RendererAssets/AssetFiles.h"
#include "Engine/Core/GUID/Guid.h"

class AssetManager;

namespace AnimationClipAssetWorkflow
{

// Converts an imported animation clip into a runtime AnimationClip.
bool BuildAnimationClip(
	AnimationClip& outClip,
	const Guid& sourceModelGuid,
	const std::vector<rendering::assets::ModelNode>& sourceNodes,
	const rendering::assets::ImportedAnimationClip& importedClips);

bool Create(
	AssetManager& assetManager,
	const std::filesystem::path& relativeDirectory,
	std::string_view name,
	const Guid& sourceModelGuid,
	const std::vector<rendering::assets::ModelNode>& sourceNodes,
	const rendering::assets::ImportedAnimationClip& sourceClip,
	Guid& outAssetGuid,
	std::string& diagnostic);
}