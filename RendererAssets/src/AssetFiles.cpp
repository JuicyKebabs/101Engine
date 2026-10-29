#include "RendererAssets/AssetFiles.h"

#include <DirectXTex.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>

namespace rendering::assets
{

namespace
{

Result Failure(const std::string& message)
{
    Result result;
    result.succeeded = false;
    result.message = message;
    return result;
}

Result ResolveFile(const std::filesystem::path& base, const std::filesystem::path& file,
    std::filesystem::path& output)
{
    if (!base.is_absolute() || file.empty())
    {
        return Failure("File import requires an absolute base directory and a nonempty path.");
    }

    std::filesystem::path resolved = file.is_absolute() ? file : base / file;
    output = resolved.lexically_normal();
    return {};
}

// Assimp uses column vectors; Renderer uses row vectors. Transpose the copied
// matrix once. aiProcess_ConvertToLeftHanded already converts handedness/UV/winding.
Matrix4x4 CopyMatrix(const aiMatrix4x4& matrix)
{
    Matrix4x4 output;
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            output.m[row][column] = matrix[static_cast<unsigned>(column)][static_cast<unsigned>(row)];
        }
    }

    return output;
}

Vector3 CopyVector(const aiVector3D& source)
{
    Vector3 value;
    value.x = source.x;
    value.y = source.y;
    value.z = source.z;
    return value;
}

}

GeometryDesc MeshData::Description() const
{
    GeometryDesc description;
    description.vertices = vertices;
    description.indices = indices;
    return description;
}

void TextureData::Description(TextureDesc& description, std::vector<TextureMipDesc>& views) const
{
    std::vector<TextureMipDesc> candidate;
    candidate.reserve(mips.size());
    for (const TextureMipData& mip : mips)
    {
        TextureMipDesc view;
        view.rowPitch = mip.rowPitch;
        view.pixels = mip.pixels;
        candidate.push_back(view);
    }

    views.swap(candidate);
    TextureDesc result;
    result.width = width;
    result.height = height;
    result.format = format;
    result.mips = views;
    description = result;
}

Result LoadModel(const std::filesystem::path& absoluteBaseDirectory,
    const std::filesystem::path& file, ModelData& output)
{
    std::filesystem::path resolved;
    Result pathResult = ResolveFile(absoluteBaseDirectory, file, resolved);
    if (!pathResult.IsSuccess())
    {
        return pathResult;
    }

    const std::u8string encodedPath = resolved.u8string();
    const std::string utf8Path(reinterpret_cast<const char*>(encodedPath.data()), encodedPath.size());
    Assimp::Importer importer;
    const unsigned flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace |
        aiProcess_GenSmoothNormals | aiProcess_GenUVCoords | aiProcess_RemoveRedundantMaterials |
        aiProcess_OptimizeMeshes | aiProcess_LimitBoneWeights | aiProcess_ConvertToLeftHanded;
    const aiScene* scene = importer.ReadFile(utf8Path, flags);
    if (!scene || !scene->mRootNode)
    {
        return Failure("Model parsing failed: " + std::string(importer.GetErrorString()));
    }

    ModelData candidate;
    candidate.meshes.resize(scene->mNumMeshes);
    for (std::size_t meshIndex = 0; meshIndex < candidate.meshes.size(); ++meshIndex)
    {
        MeshData& mesh = candidate.meshes[meshIndex];
        const aiMesh* imported = scene->mMeshes[meshIndex];
        if (!imported || !imported->HasPositions() || !imported->HasNormals())
        {
            mesh.diagnostic = "SubMesh has no required positions/normals.";
            continue;
        }

        mesh.vertices.reserve(imported->mNumVertices);
        for (unsigned vertexIndex = 0; vertexIndex < imported->mNumVertices; ++vertexIndex)
        {
            MeshVertex vertex;
            vertex.position = CopyVector(imported->mVertices[vertexIndex]);
            vertex.normal = CopyVector(imported->mNormals[vertexIndex]);
            if (imported->HasTangentsAndBitangents())
            {
                vertex.tangent = CopyVector(imported->mTangents[vertexIndex]);
            }

            if (imported->HasTextureCoords(0))
            {
                vertex.uv.x = imported->mTextureCoords[0][vertexIndex].x;
                vertex.uv.y = imported->mTextureCoords[0][vertexIndex].y;
            }

            vertex.color.x = 1;
            vertex.color.y = 1;
            vertex.color.z = 1;
            vertex.color.w = 1;
            if (imported->HasVertexColors(0))
            {
                // Assimp vertex colors are passed as authored numeric values.
                // This helper's contract treats these non-texture values as linear.
                const aiColor4D& color = imported->mColors[0][vertexIndex];
                vertex.color.x = color.r;
                vertex.color.y = color.g;
                vertex.color.z = color.b;
                vertex.color.w = color.a;
            }

            mesh.vertices.push_back(vertex);
        }

        mesh.valid = !mesh.vertices.empty();
        for (unsigned faceIndex = 0; faceIndex < imported->mNumFaces; ++faceIndex)
        {
            const aiFace& face = imported->mFaces[faceIndex];
            if (face.mNumIndices != 3)
            {
                mesh.valid = false;
                mesh.diagnostic = "Triangulation left a non-triangle face.";
                break;
            }

            for (unsigned corner = 0; corner < 3; ++corner)
            {
                mesh.indices.push_back(face.mIndices[corner]);
            }
        }

        if (imported->mMaterialIndex < scene->mNumMaterials)
        {
            aiString texture;
            const aiMaterial* material = scene->mMaterials[imported->mMaterialIndex];
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texture) == AI_SUCCESS)
            {
                const std::string textureName = texture.C_Str();
                if (!textureName.empty() && textureName.front() != '*')
                {
                    mesh.texturePath = (resolved.parent_path() / std::filesystem::u8path(textureName)).lexically_normal();
                }
                else
                {
                    // Preserve Geometry even when an unsupported embedded image
                    // cannot be decoded. Engine records an Invalid Texture result.
                    mesh.texturePath = resolved.parent_path() / L"__unsupported_embedded_texture__";
                    mesh.diagnostic += " Embedded Texture import is unavailable.";
                }
            }
        }
    }

    std::function<void(const aiNode*, std::int32_t, const Matrix4x4&)> visit;
    visit = [&](const aiNode* node, std::int32_t parent, const Matrix4x4& parentToModel)
    {
        ModelNode copiedNode;
        copiedNode.name = node->mName.C_Str();
        copiedNode.parentIndex = parent;
        copiedNode.localTransform = CopyMatrix(node->mTransformation);
        const std::size_t nodeIndex = candidate.nodes.size();
        candidate.nodes.push_back(copiedNode);
        const Matrix4x4 localToModel = copiedNode.localTransform * parentToModel;
        for (unsigned placementIndex = 0; placementIndex < node->mNumMeshes; ++placementIndex)
        {
            if (node->mMeshes[placementIndex] >= candidate.meshes.size())
            {
                continue;
            }

            MeshPlacement placement;
            placement.meshIndex = node->mMeshes[placementIndex];
            placement.nodeIndex = nodeIndex;
            placement.localToModel = localToModel;
            candidate.placements.push_back(placement);
        }

        for (unsigned childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
        {
            visit(node->mChildren[childIndex], static_cast<std::int32_t>(nodeIndex), localToModel);
        }
    };

    Matrix4x4 modelRoot = Matrix4x4::Identity();
    visit(scene->mRootNode, -1, modelRoot);
    if (scene->mNumAnimations != 0)
    {
        const aiAnimation& animation = *scene->mAnimations[0];
        candidate.animationDuration = animation.mDuration;
        candidate.ticksPerSecond = animation.mTicksPerSecond == 0 ? 30 : animation.mTicksPerSecond;
        for (unsigned channelIndex = 0; channelIndex < animation.mNumChannels; ++channelIndex)
        {
            const aiNodeAnim& source = *animation.mChannels[channelIndex];
            AnimationChannel channel;
            channel.nodeName = source.mNodeName.C_Str();
            const auto copyVectors = [](const aiVectorKey* keys, unsigned count, std::vector<AnimationVectorKey>& destination)
            {
                destination.reserve(count);
                for (unsigned index = 0; index < count; ++index)
                {
                    AnimationVectorKey key;
                    key.time = keys[index].mTime;
                    key.value = CopyVector(keys[index].mValue);
                    destination.push_back(key);
                }
            };

            copyVectors(source.mPositionKeys, source.mNumPositionKeys, channel.positions);
            copyVectors(source.mScalingKeys, source.mNumScalingKeys, channel.scales);
            for (unsigned keyIndex = 0; keyIndex < source.mNumRotationKeys; ++keyIndex)
            {
                const aiQuatKey& sourceKey = source.mRotationKeys[keyIndex];
                AnimationRotationKey key;
                key.time = sourceKey.mTime;
                key.value.x = sourceKey.mValue.x;
                key.value.y = sourceKey.mValue.y;
                key.value.z = sourceKey.mValue.z;
                key.value.w = sourceKey.mValue.w;
                channel.rotations.push_back(key);
            }

            candidate.animationChannels.push_back(std::move(channel));
        }
    }

    output = std::move(candidate);
    return {};
}

Result LoadTexture(const std::filesystem::path& absoluteBaseDirectory,
    const std::filesystem::path& file, TextureData& output, bool colorTexture)
{
    std::filesystem::path resolved;
    Result pathResult = ResolveFile(absoluteBaseDirectory, file, resolved);
    if (!pathResult.IsSuccess())
    {
        return pathResult;
    }

    DirectX::ScratchImage decoded;
    DirectX::TexMetadata metadata;
    HRESULT loaded = E_FAIL;
    std::wstring extension = resolved.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
    if (extension == L".dds")
    {
        loaded = DirectX::LoadFromDDSFile(resolved.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, decoded);
    }
    else if (extension == L".tga")
    {
        loaded = DirectX::LoadFromTGAFile(resolved.c_str(), DirectX::TGA_FLAGS_NONE, &metadata, decoded);
    }
    else
    {
        loaded = DirectX::LoadFromWICFile(resolved.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, &metadata, decoded);
    }

    if (FAILED(loaded))
    {
        return Failure("Texture decode failed, HRESULT=" + std::to_string(static_cast<unsigned long>(loaded)));
    }

    const bool oneImage = metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D && metadata.arraySize == 1;
    if (!oneImage || metadata.IsCubemap())
    {
        return Failure("Only a single 2D Texture with mip levels is supported.");
    }

    if (metadata.width > UINT32_MAX || metadata.height > UINT32_MAX)
    {
        return Failure("Texture dimensions exceed the public descriptor range.");
    }

    DirectX::ScratchImage converted;
    // TextureDesc's colorTexture choice controls sampling interpretation. Keep
    // encoded file RGB bytes during format/swizzle conversion: DirectXTex would
    // otherwise infer an sRGB-to-linear transfer from a DDS *_SRGB source, after
    // which a second sRGB decode at sampling would incorrectly darken the image.
    const DXGI_FORMAT storageFormat = DirectX::MakeLinear(metadata.format);
    if (storageFormat != metadata.format)
    {
        if (!decoded.OverrideFormat(storageFormat))
        {
            return Failure("Cannot preserve encoded Texture storage format.");
        }

        metadata.format = storageFormat;
    }

    if (DirectX::IsCompressed(metadata.format))
    {
        loaded = DirectX::Decompress(decoded.GetImages(), decoded.GetImageCount(), metadata,
            DXGI_FORMAT_R8G8B8A8_UNORM, converted);
    }
    else if (metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM)
    {
        loaded = DirectX::Convert(decoded.GetImages(), decoded.GetImageCount(), metadata,
            DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted);
    }

    if (FAILED(loaded))
    {
        return Failure("Texture conversion to RGBA8 failed.");
    }

    const DirectX::ScratchImage& image = converted.GetImageCount() != 0 ? converted : decoded;
    TextureData candidate;
    candidate.width = static_cast<std::uint32_t>(metadata.width);
    candidate.height = static_cast<std::uint32_t>(metadata.height);
    candidate.format = colorTexture ? TextureFormat::RGBA8_sRGB : TextureFormat::RGBA8_UNorm;
    for (std::size_t mipIndex = 0; mipIndex < metadata.mipLevels; ++mipIndex)
    {
        const DirectX::Image* source = image.GetImage(mipIndex, 0, 0);
        if (!source)
        {
            return Failure("Decoded Texture mip is missing.");
        }

        TextureMipData mip;
        mip.rowPitch = source->rowPitch;
        mip.pixels.resize(source->slicePitch);
        std::memcpy(mip.pixels.data(), source->pixels, source->slicePitch);
        candidate.mips.push_back(std::move(mip));
    }

    output = std::move(candidate);
    return {};
}

Result LoadShader(const std::filesystem::path& absoluteBaseDirectory,
    const std::filesystem::path& file, std::vector<std::byte>& output)
{
    std::filesystem::path resolved;
    Result pathResult = ResolveFile(absoluteBaseDirectory, file, resolved);
    if (!pathResult.IsSuccess())
    {
        return pathResult;
    }

    std::ifstream input(resolved, std::ios::binary | std::ios::ate);
    if (!input)
    {
        return Failure("Cannot open compiled Shader.");
    }

    const std::streamoff size = input.tellg();
    if (size <= 0 || static_cast<std::uintmax_t>(size) > (std::numeric_limits<std::size_t>::max)())
    {
        return Failure("Compiled Shader size is invalid.");
    }

    std::vector<std::byte> candidate(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(candidate.data()), size))
    {
        return Failure("Cannot read complete compiled Shader.");
    }

    output.swap(candidate);
    return {};
}

}
