#include "RenderAssetResources.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Debug/Debug.h"
#include "RendererAssets/AssetFiles.h"

#include <vector>

RenderAssetResources::~RenderAssetResources()
{
    Finalize();
}

bool RenderAssetResources::Initialize(rendering::Renderer& renderer,
    const std::filesystem::path& absoluteShaderDirectory)
{
    if (m_renderer || !absoluteShaderDirectory.is_absolute())
    {
        return false;
    }

    m_renderer = &renderer;
    try
    {
        const std::array<const char*, 3> shaderFiles{ "MeshVS.cso", "LitPS.cso", "TexturePS.cso" };
        for (std::size_t index = 0; index < shaderFiles.size(); ++index)
        {
            std::vector<std::byte> bytes;
            rendering::Result loaded = rendering::assets::LoadShader(absoluteShaderDirectory, shaderFiles[index], bytes);
            if (!loaded.IsSuccess())
            {
                DBG("Standard Shader: %s", loaded.message.c_str());
                Finalize();
                return false;
            }

            rendering::ShaderDesc description;
            description.stage = index == 0 ? rendering::ShaderStage::Vertex : rendering::ShaderStage::Pixel;
            description.bytecode = bytes;
            auto registration = renderer.RegisterShader(description);
            if (!registration.result.IsSuccess())
            {
                DBG("Standard Shader: %s", registration.result.message.c_str());
                Finalize();
                return false;
            }

            m_shaders[index] = registration.handle;
        }

        for (std::size_t index = 0; index < m_pipelines.size(); ++index)
        {
            const bool textured = index >= 2;
            const bool screen = index % 2 != 0;
            rendering::PipelineDesc description;
            description.vertexShader = m_shaders[0];
            description.pixelShader = m_shaders[textured ? 2 : 1];
            description.blend = textured ? rendering::BlendMode::Alpha : rendering::BlendMode::Opaque;
            description.cull = rendering::CullMode::None;
            description.depth = rendering::DepthMode::Disabled;
            if (!screen)
            {
                description.depth = textured ? rendering::DepthMode::ReadOnly : rendering::DepthMode::ReadWrite;
            }

            auto registration = renderer.RegisterPipeline(description);
            if (!registration.result.IsSuccess())
            {
                DBG("Standard Pipeline: %s", registration.result.message.c_str());
                Finalize();
                return false;
            }

            m_pipelines[index] = registration.handle;
        }

        const std::array<std::byte, 4> whitePixels{
            std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}
        };
        rendering::TextureMipDesc mip;
        mip.pixels = whitePixels;
        mip.rowPitch = 4;
        rendering::TextureDesc whiteDescription;
        whiteDescription.width = 1;
        whiteDescription.height = 1;
        whiteDescription.format = rendering::TextureFormat::RGBA8_sRGB;
        whiteDescription.mips = std::span(&mip, 1);
        auto whiteRegistration = renderer.RegisterTexture(whiteDescription);
        if (!whiteRegistration.status.IsSuccess())
        {
            Finalize();
            return false;
        }

        m_white = whiteRegistration.handle;

        std::array<rendering::MeshVertex, 4> vertices{};
        for (std::size_t index = 0; index < vertices.size(); ++index)
        {
            rendering::MeshVertex vertex;
            vertex.position.x = index % 2 == 0 ? -0.5f : 0.5f;
            vertex.position.y = index < 2 ? 0.5f : -0.5f;
            vertex.normal.z = 1;
            vertex.tangent.x = 1;
            vertex.uv.x = index % 2 == 0 ? 0 : 1;
            vertex.uv.y = index < 2 ? 0 : 1;
            vertex.color.x = 1;
            vertex.color.y = 1;
            vertex.color.z = 1;
            vertex.color.w = 1;
            vertices[index] = vertex;
        }

        const std::array<std::uint32_t, 6> indices{ 0, 1, 2, 1, 3, 2 };
        rendering::GeometryDesc quadDescription;
        quadDescription.vertices = vertices;
        quadDescription.indices = indices;
        auto quadRegistration = renderer.RegisterGeometry(quadDescription);
        if (!quadRegistration.status.IsSuccess())
        {
            Finalize();
            return false;
        }

        m_quad = quadRegistration.handle;
        return true;
    }
    catch (const std::exception& exception)
    {
        DBG("Standard render resources: %s", exception.what());
        Finalize();
        return false;
    }
}

void RenderAssetResources::Finalize() noexcept
{
    if (!m_renderer)
    {
        return;
    }

    // Logical destruction happens before Renderer/Window shutdown. GPU Fence
    // collection remains entirely inside Renderer, including partial startup.
    for (auto handle : m_pipelines)
    {
        if (handle != rendering::PipelineHandle{})
        {
            const auto result = m_renderer->DestroyPipeline(handle);
            if (!result.IsSuccess())
            {
                DBG("Standard Pipeline destruction: %s", result.message.c_str());
            }
        }
    }

    for (auto handle : m_shaders)
    {
        if (handle != rendering::ShaderHandle{})
        {
            const auto result = m_renderer->DestroyShader(handle);
            if (!result.IsSuccess())
            {
                DBG("Standard Shader destruction: %s", result.message.c_str());
            }
        }
    }

    if (m_quad != rendering::GeometryHandle{})
    {
        const auto result = m_renderer->DestroyGeometry(m_quad);
        if (!result.IsSuccess())
        {
            DBG("Shared Quad destruction: %s", result.message.c_str());
        }
    }

    if (m_white != rendering::TextureHandle{})
    {
        const auto result = m_renderer->DestroyTexture(m_white);
        if (!result.IsSuccess())
        {
            DBG("White Texture destruction: %s", result.message.c_str());
        }
    }

    m_pipelines = {};
    m_shaders = {};
    m_quad = rendering::GeometryHandle{};
    m_white = rendering::TextureHandle{};
    m_renderer = nullptr;
}

rendering::PipelineHandle RenderAssetResources::MeshPipeline(bool screen) const noexcept
{
    return m_pipelines[screen ? 1 : 0];
}

rendering::PipelineHandle RenderAssetResources::TexturedPipeline(bool screen) const noexcept
{
    return m_pipelines[screen ? 3 : 2];
}

rendering::MaterialDesc RenderAssetResources::MakeMaterial(bool mesh, bool screen, rendering::TextureHandle texture,
    const Vector2& uvScale, const Vector2& uvOffset, const Vector2& pivot, bool flipX, bool flipY) const
{
    rendering::Vector4 white;
    white.x = 1;
    white.y = 1;
    white.z = 1;
    white.w = 1;
    rendering::MaterialParameter baseColor;
    baseColor.name = "BaseColor";
    baseColor.value = white;

    rendering::Vector4 uv;
    uv.x = uvScale.x;
    uv.y = uvScale.y;
    uv.z = uvOffset.x;
    uv.w = uvOffset.y;
    rendering::MaterialParameter uvParameter;
    uvParameter.name = "UVTransform";
    uvParameter.value = uv;

    rendering::Vector4 localQuad;
    localQuad.x = flipX ? -1.0f : 1.0f;
    localQuad.y = flipY ? -1.0f : 1.0f;
    localQuad.z = (0.5f - pivot.x) * localQuad.x;
    localQuad.w = (0.5f - pivot.y) * localQuad.y;
    rendering::MaterialParameter localParameter;
    localParameter.name = "LocalQuadTransform";
    localParameter.value = localQuad;

    rendering::MaterialParameter textureParameter;
    textureParameter.name = "BaseTexture";
    textureParameter.value = texture;

    rendering::MaterialDesc description;
    description.pipeline = mesh ? MeshPipeline(screen) : TexturedPipeline(screen);
    description.parameters.push_back(baseColor);
    description.parameters.push_back(uvParameter);
    description.parameters.push_back(localParameter);
    description.parameters.push_back(textureParameter);
    return description;
}
