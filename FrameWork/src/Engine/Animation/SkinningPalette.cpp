#include "SkinningPalette.h"

#include <cmath>
#include <utility>

namespace
{

bool IsFinite(const Matrix4x4& matrix)
{
	for (const auto& row : matrix.m)
	{
		for (float value : row)
		{
			if (!std::isfinite(value))
			{
				return false;
			}
		}
	}

	return true;
}

}

namespace animation::skinning
{

bool BuildBindPose(
	std::span<const rendering::assets::ModelNode> nodes,
	std::vector<Matrix4x4>& outBindPose)
{
	return BuildModelPose(nodes, {}, outBindPose);
}

bool BuildPalette(
	std::span<const rendering::assets::SkinBone> bones,
	std::span<const Matrix4x4> nodeToModel,
	const Matrix4x4& meshBindToModel,
	std::vector<Matrix4x4>& output)
{
	if (bones.empty())
	{
		output.clear();
		return true;
	}

	const float determinant = meshBindToModel.Determinant();

	if (!std::isfinite(determinant) || determinant == 0.0f)
	{
		return false;
	}

	const Matrix4x4 modelToMeshBind = meshBindToModel.Inverse();

	if (!IsFinite(modelToMeshBind))
	{
		return false;
	}

	std::vector<Matrix4x4> palette;
	palette.reserve(bones.size());

	for (const auto& bone : bones)
	{
		if (bone.nodeIndex >= nodeToModel.size())
		{
			return false;
		}

		const Matrix4x4& meshToBoneBind = bone.meshToBoneBind;
		const Matrix4x4 matrix = meshToBoneBind * nodeToModel[bone.nodeIndex] * modelToMeshBind;
	
		if (!IsFinite(matrix))
		{
			return false;
		}

		palette.push_back(matrix);
	}

	output = std::move(palette);
	return true;
}

bool BuildModelPose(
	std::span<const rendering::assets::ModelNode> nodes,
	std::span<const Matrix4x4> localPose,
	std::vector<Matrix4x4>& output)
{
	if (nodes.empty() || (!localPose.empty() && localPose.size() != nodes.size()))
	{
		return false;
	}

	std::vector<Matrix4x4> nodeToModel;
	nodeToModel.reserve(nodes.size());

	for (std::size_t index = 0; index < nodes.size(); ++index)
	{
		const auto& node = nodes[index];
		Matrix4x4 matrix = localPose.empty() ? node.localTransform : localPose[index];

		if (nodeToModel.empty())
		{
			// Only root nodes can have no parent.
			if (node.parentIndex != -1)
			{
				return false;
			}
		}
		else
		{
			if (node.parentIndex < 0 || static_cast<std::size_t>(node.parentIndex) >= nodeToModel.size())
			{
				return false;
			}

			// Multiply the local transform by the parent's model transform
			// to get the model transform for this node.
			matrix = matrix * nodeToModel[node.parentIndex];
		}

		if (!IsFinite(matrix))
		{
			return false;
		}

		nodeToModel.push_back(matrix);
	}

	output = std::move(nodeToModel);
	return true;
}

}