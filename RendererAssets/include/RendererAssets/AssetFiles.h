#pragma once

#include "Renderer/DrawTypes.h"
#include "Renderer/GeometryDesc.h"
#include "Renderer/TextureDesc.h"

#include <array>
#include <filesystem>
#include <vector>

namespace rendering::assets
{

	inline constexpr std::size_t kMaxVertexBoneInfluences = 4;	// Maximum 4 bone influences per vertex is supported.

struct SkinBone
{
	std::size_t nodeIndex = 0;							// Index of the node in the model's node hierarchy that this bone corresponds to.
	Matrix4x4 meshToBoneBind = Matrix4x4::Identity();	// Transform from mesh space to bone space at bind pose.
};

struct VertexSkinWeight
{
	std::array<std::uint32_t, kMaxVertexBoneInfluences> boneIndices{};	// Which boines influence this vertex (represented by their indices in the MeshSkinData::bones array).
	std::array<float, kMaxVertexBoneInfluences> boneWeights{};			// How much each bone influences this vertex. The sum of all weights should be 1.0f.
};

struct MeshSkinData
{
	std::vector<SkinBone> bones;					// Every bone in the mesh, with its corresponding node index and bind pose transform.
	std::vector<VertexSkinWeight> vertexWeights;	// The skinning weights for each vertex in the mesh. The size of this vector should be equal to the number of vertices in the mesh.
	bool valid = true;
};

// CPU-owned import results. No GUID, Scene, Engine Context, native GPU object,
// or Assimp pointer crosses this optional helper target's public boundary.
struct MeshData
{
	std::vector<MeshVertex> vertices;
	std::vector<std::uint32_t> indices;
	std::filesystem::path texturePath;
	MeshSkinData skin;
	std::string diagnostic;
	bool valid = false;

	GeometryDesc Description() const;
};

struct MeshPlacement
{
	std::size_t meshIndex = 0;
	std::size_t nodeIndex = 0;
	Matrix4x4 localToModel = Matrix4x4::Identity();
};

template<class T>
struct AnimationKey
{
	double time = 0;
	T value;
};

using AnimationVectorKey = AnimationKey<Vector3>;
using AnimationRotationKey = AnimationKey<Quaternion>;

struct AnimationChannel
{
	std::string nodeName;
	std::vector<AnimationVectorKey> positions;
	std::vector<AnimationRotationKey> rotations;
	std::vector<AnimationVectorKey> scales;
};

struct ModelNode
{
	std::string name;
	std::int32_t parentIndex = -1;
	Matrix4x4 localTransform = Matrix4x4::Identity();
};

struct ImportedAnimationClip
{
	std::string name;
	double durationTicks = 0;
	double ticksPerSecond = 0;
	std::vector<AnimationChannel> channels;
};

struct ModelData
{
	std::vector<MeshData> meshes;
	std::vector<MeshPlacement> placements;

	// Preserve the existing first-clip CPU entry point without coupling the
	// file helper to a Component or expanding animation formats in this phase.
	std::vector<ModelNode> nodes;
	std::vector<ImportedAnimationClip> animationClips;
	double animationDuration = 0;
	double ticksPerSecond = 30;
};

struct TextureMipData
{
	std::vector<std::byte> pixels;
	std::size_t rowPitch = 0;
};

struct TextureData
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	TextureFormat format = TextureFormat::RGBA8_sRGB;
	std::vector<TextureMipData> mips;

	// Description spans borrow this object and the caller-owned mip view list.
	// Keep both alive and immutable until explicit RegisterTexture returns.
	void Description(TextureDesc& description, std::vector<TextureMipDesc>& views) const;
};

// Helper API to load a resources from a file.
Result LoadModel(const std::filesystem::path& absoluteBaseDirectory, const std::filesystem::path& file, ModelData& output);
Result LoadTexture(const std::filesystem::path& absoluteBaseDirectory, const std::filesystem::path& file, TextureData& output, bool colorTexture = true);
Result LoadShader(const std::filesystem::path& absoluteBaseDirectory, const std::filesystem::path& file, std::vector<std::byte>& output);

}
