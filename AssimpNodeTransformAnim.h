#pragma once
#include <DirectXMath.h>
#include <assimp/scene.h>
#include <assimp/matrix4x4.h>
#include <assimp/quaternion.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <cmath>

// Node structure
struct Node
{
	std::string name = "";			// Node name
	int parent = -1;				// Parent node index
	std::vector<int> children = {};	// Child node indices
	aiMatrix4x4 baseLocalTransform{};	// Base local transformation matrix
};

// Channel structure
struct Channel
{
	std::vector<aiVectorKey> positionKeys;		// Position keyframes
	std::vector<aiQuatKey> rotationKeys;		// Rotation keyframes
	std::vector<aiVectorKey> scalingKeys;		// Scaling keyframes
};

// Clip structure
struct Clip
{
	double duration = 0.0;								// Animation clip duration
	double ticksPerSecond = 30.0;						// Ticks per second
	std::unordered_map<std::string, Channel> channels;	// Animation channels
};

// NodeAnimationAsset structure
struct NodeAnimationAsset
{
	std::vector<Node> nodes;								// Nodes
	std::unordered_map<std::string, int> nodeIndexByName{};	// Node name to index mapping
	std::vector<int> meshNodeIndices = { -1 };				// Mesh node indices
	Clip clip0{};											// Animation clip 0
};

// Function to build node tree from aiScene
static void BuildNodeTree(const aiScene* scene, NodeAnimationAsset& out)
{
	out.nodes.clear();									// Clear existing nodes
	out.nodeIndexByName.clear();						// Clear existing name to index mapping
	out.meshNodeIndices.assign(scene->mNumMeshes, -1);	// Initialize mesh node indices

	// Recursive function to build node tree
	std::function<int(aiNode*, int)> rec = [&](aiNode* node, int parent) -> int
		{
			// Create new node
			int index = (int)out.nodes.size();					// Current node index
			Node newNode;										// New node
			newNode.name = node->mName.C_Str();					// Set node name
			newNode.parent = parent;							// Set parent index
			newNode.baseLocalTransform = node->mTransformation; // Set local transform
			out.nodes.push_back(newNode);						// Add new node to the list
			out.nodeIndexByName[newNode.name] = index;			// Map node name to index

			// Map meshes to this node
			for (unsigned i = 0; i < node->mNumMeshes; ++i)
			{
				unsigned meshIndex = node->mMeshes[i];	// Get mesh index
				out.meshNodeIndices[meshIndex] = index;	// Map mesh index to node index
			}

			// Process child nodes
			out.nodes[index].children.reserve(node->mNumChildren);	// Reserve space for children
			for (unsigned i = 0; i < node->mNumChildren; ++i)
			{
				int childIndex = rec(node->mChildren[i], index);	// Recursively process child
				out.nodes[index].children.push_back(childIndex);	// Add child index to current node
			}
			return index;
		};

	rec(scene->mRootNode, -1);	// Start recursion from root node
}

// Function to build animation clip 0 from aiScene
static void BuildClip0(const aiScene* scene, NodeAnimationAsset& out)
{
	if (scene->mNumAnimations == 0) return;	// No animations to process

	// Process the first animation
	const aiAnimation* anim = scene->mAnimations[0];					// Get the first animation
	out.clip0.duration = anim->mDuration;								// Set clip duration
	out.clip0.ticksPerSecond = 
		(anim->mTicksPerSecond != 0.0) ? anim->mTicksPerSecond : 30.0;	// Set ticks per second
	out.clip0.channels.clear();											// Clear existing channels

	// Process each channel
	for(unsigned i = 0; i < anim->mNumChannels; ++i)
	{
		// Get source channel
		const aiNodeAnim* channel = anim->mChannels[i];	// Get channel
		Channel dstChannel;								// Destination channel

		// Copy keyframes
		dstChannel.positionKeys.assign(channel->mPositionKeys, channel->mPositionKeys + channel->mNumPositionKeys);	// Copy position keys
		dstChannel.rotationKeys.assign(channel->mRotationKeys, channel->mRotationKeys + channel->mNumRotationKeys);	// Copy rotation keys
		dstChannel.scalingKeys.assign(channel->mScalingKeys, channel->mScalingKeys + channel->mNumScalingKeys);		// Copy scaling keys
		
		// Store channel in clip
		out.clip0.channels[channel->mNodeName.C_Str()] = std::move(dstChannel);
	}
}

// Function to find key index for given time (vector keys)
static int FindKeyIndex(const std::vector<aiVectorKey>& keys, double t)
{
	// Handle edge cases
	// If there are no keys, return -1
	if(keys.empty()) return -1;

	// If time is before the first key, return the first index
	if(t < keys[0].mTime) return 0;

	// Iterate through keys to find the correct index
	for(int i = 0; i < static_cast<int>(keys.size()) - 1; ++i)
	{
		if(t < keys[i + 1].mTime)
			return i;
	}

	// If time exceeds all key times, return the last valid index
	return (int)keys.size() - 2;
}

// Function to find key index for given time (quaternion keys)
static int FindKeyIndex(const std::vector<aiQuatKey>& keys, double t)
{
	// Handle edge cases
	// If there are no keys, return -1
	if(keys.empty()) return -1;

	// If time is before the first key, return the first index
	if(t < keys[0].mTime) return 0;

	// Iterate through keys to find the correct index
	for(int i = 0; i < static_cast<int>(keys.size()) - 1; ++i)
	{
		if(t < keys[i + 1].mTime)
			return i;
	}

	// If time exceeds all key times, return the last valid index
	return (int)keys.size() - 2;
}

// Linear interpolation for aiVector3D
static aiVector3D LerpVec3(const aiVector3D& a, const aiVector3D& b, float f)
{
	return aiVector3D(
		a.x + (b.x - a.x) * f,
		a.y + (b.y - a.y) * f,
		a.z + (b.z - a.z) * f
	);
}

// Function to evaluate position at time t
static aiVector3D EvaluatePosition(const Channel& channel, double t)
{
	// Handle edge cases
	if(channel.positionKeys.empty()) return aiVector3D(0.0f, 0.0f, 0.0f);			// Default position if no keys
	if (channel.positionKeys.size() == 1) return channel.positionKeys[0].mValue;	// Single key

	// Find surrounding keys and interpolate
	int i = FindKeyIndex(channel.positionKeys, t);							// Find key index
	const auto& key0 = channel.positionKeys[i];								// Key 0
	const auto& key1 = channel.positionKeys[i + 1];							// Key 1
	double dt = key1.mTime - key0.mTime;									// Delta time
	float f =(dt > 0.0) ? static_cast<float>((t - key0.mTime) / dt) : 0.0f; // Interpolation factor
	return LerpVec3(key0.mValue, key1.mValue, f);							// Interpolated position
}

// Function to evaluate rotation at time t
static aiQuaternion EvaluateRotation(const Channel& channel, double t)
{
	// Handle edge cases
	if(channel.rotationKeys.empty()) return aiQuaternion();							// Default rotation if no keys
	if (channel.rotationKeys.size() == 1) return channel.rotationKeys[0].mValue;	// Single key

	// Find surrounding keys and interpolate
	int i = FindKeyIndex(channel.rotationKeys, t);							// Find key index
	const auto& key0 = channel.rotationKeys[i];								// Key 0
	const auto& key1 = channel.rotationKeys[i + 1];							// Key 1
	double dt = key1.mTime - key0.mTime;									// Delta time
	float f = (dt > 0.0) ? static_cast<float>((t - key0.mTime) / dt) : 0.0f; // Interpolation factor

	// Slerp between the two quaternions
	aiQuaternion out;
	aiQuaternion::Interpolate(out, key0.mValue, key1.mValue, f);	// Interpolate
	out.Normalize();												// Normalize result
	return out;														// Interpolated rotation
}

// Function to evaluate scaling at time t
static aiVector3D EvaluateScaling(const Channel& channel, double t)
{
	// Handle edge cases
	if(channel.scalingKeys.empty()) return aiVector3D(1.0f, 1.0f, 1.0f);		// Default scaling if no keys
	if (channel.scalingKeys.size() == 1) return channel.scalingKeys[0].mValue;	// Single key

	// Find surrounding keys and interpolate
	int i = FindKeyIndex(channel.scalingKeys, t);								// Find key index
	const auto& key0 = channel.scalingKeys[i];									// Key 0
	const auto& key1 = channel.scalingKeys[i + 1];								// Key 1
	double dt = key1.mTime - key0.mTime;										// Delta time
	float f = (dt > 0.0) ? static_cast<float>((t - key0.mTime) / dt) : 0.0f;	// Interpolation factor
	return LerpVec3(key0.mValue, key1.mValue, f);								// Interpolated scaling
}

// Function to evaluate local transformation matrix at time t for a node
static aiMatrix4x4 EvaluateLocalTransform(double t, const Node& node, const NodeAnimationAsset& asset)
{
	// Find channel for the node
	auto it = asset.clip0.channels.find(node.name);
	if (it == asset.clip0.channels.end()) return node.baseLocalTransform;

	// Evaluate transformation components
	const Channel& channel = it->second;
	aiVector3D T = EvaluatePosition(channel, t);	// Evaluate position
	aiQuaternion R = EvaluateRotation(channel, t);	// Evaluate rotation
	aiVector3D S = EvaluateScaling(channel, t);		// Evaluate scaling
	return aiMatrix4x4(S, R, T);					// Compose transformation matrix
}

// NodeAnimator structure
struct NodeAnimator
{
	const NodeAnimationAsset* asset = nullptr;	// Pointer to animation asset
	double time = 0.0;							// Current time
	std::vector<aiMatrix4x4> globalTransforms;	// Global transformation matrices

	NodeAnimator() { 
		asset = new NodeAnimationAsset(); time = 0.0; };

	// Bind the animator to an asset
	void Bind(const NodeAnimationAsset* a)
	{
		asset = a;
		time = 0.0;
		globalTransforms.resize(asset->nodes.size(), aiMatrix4x4());
	}

	// Update the animator by delta time
	void Update(double dt)
	{
		if (!asset) return;	// No asset bound

		// Update time
		time += dt;
		double duration = asset->clip0.duration;
		auto ticksPerSecond = asset->clip0.ticksPerSecond;
		if (duration <= 0.0) return;

		// Wrap time within duration
		double timeInTicks = std::fmod(time * ticksPerSecond, duration);

		// Recursive function to compute global transforms
		std::function<void(int, const aiMatrix4x4&)> rec = [&](int nodeIndex, const aiMatrix4x4& parentTransform)
		{
			// Get node and compute transforms
			const Node& node = asset->nodes[nodeIndex];
			aiMatrix4x4 localTransform = EvaluateLocalTransform(timeInTicks, node, *asset);
			aiMatrix4x4 globalTransform = parentTransform * localTransform;
			globalTransforms[nodeIndex] = globalTransform;

			// Process children
			for (int childIndex : node.children)
			{
				rec(childIndex, globalTransform);
			}
		};

		// Start recursion from root nodes
		for(int i = 0; i < static_cast<int>(asset->nodes.size()); ++i)
		{
			if(asset->nodes[i].parent == -1)
			{
				rec(i, aiMatrix4x4()); // Start recursion from root nodes
			}
		}
	}

	// Get global transform for a specific mesh
	const aiMatrix4x4& GetMeshGlobalTransform(int meshIndex) const
	{
		int nodeIndex = asset->meshNodeIndices[meshIndex];
		return globalTransforms[nodeIndex];
	}
};

// Function to convert aiMatrix4x4 to DirectX::XMMATRIX
static DirectX::XMMATRIX AiMatrix4x4ToXMMatrix(const aiMatrix4x4& mat)
{
	return DirectX::XMMATRIX(
		mat.a1, mat.b1, mat.c1, mat.d1,
		mat.a2, mat.b2, mat.c2, mat.d2,
		mat.a3, mat.b3, mat.c3, mat.d3,
		mat.a4, mat.b4, mat.c4, mat.d4
	);
}