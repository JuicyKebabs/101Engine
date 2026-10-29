#pragma once

#include "Renderer/DrawTypes.h"
#include "Renderer/GeometryDesc.h"
#include "Renderer/TextureDesc.h"

#include <filesystem>
#include <vector>

namespace rendering::assets
{

// CPU-owned import results. No GUID, Scene, Engine Context, native GPU object,
// or Assimp pointer crosses this optional helper target's public boundary.
struct MeshData
{
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::filesystem::path texturePath;
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

struct AnimationVectorKey
{
    double time = 0;
    Vector3 value;
};

struct AnimationRotationKey
{
    double time = 0;
    Quaternion value;
};

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

struct ModelData
{
    std::vector<MeshData> meshes;
    std::vector<MeshPlacement> placements;
    // Preserve the existing first-clip CPU entry point without coupling the
    // file helper to a Component or expanding animation formats in this phase.
    std::vector<ModelNode> nodes;
    std::vector<AnimationChannel> animationChannels;
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

Result LoadModel(const std::filesystem::path& absoluteBaseDirectory,
    const std::filesystem::path& file, ModelData& output);
Result LoadTexture(const std::filesystem::path& absoluteBaseDirectory,
    const std::filesystem::path& file, TextureData& output, bool colorTexture = true);
Result LoadShader(const std::filesystem::path& absoluteBaseDirectory,
    const std::filesystem::path& file, std::vector<std::byte>& output);

}
