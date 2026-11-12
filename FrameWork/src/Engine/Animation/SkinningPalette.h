#pragma once

#include "Engine/Core/Math/Math.h"
#include "RendererAssets/AssetFiles.h"

#include <span>
#include <vector>

namespace animation::skinning
{

bool BuildBindPose(
	std::span<const rendering::assets::ModelNode> nodes,
	std::vector<Matrix4x4>& outBindPose
);

bool BuildPalette(
	std::span<const rendering::assets::SkinBone> bones,
	std::span<const Matrix4x4> nodeToModel,
	const Matrix4x4& meshBindToModel,
	std::vector<Matrix4x4>& outPalette
);

bool BuildModelPose(
	std::span<const rendering::assets::ModelNode> nodes,
	std::span<const Matrix4x4> localPose,
	std::vector<Matrix4x4>& output);

}