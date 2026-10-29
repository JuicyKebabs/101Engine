// Missing project assets must still be visible even when no external file exists.
// These registrations are private Renderer residents, never Scene-owned handles.
#include "DrawSystem.h"
#include "Failure.h"

#include <array>
#include <cstring>
#include <d3dcompiler.h>

namespace rendering::detail
{

namespace
{

// Compile only the private emergency shaders. User shaders retain the explicit
// compiled-bytecode API; there is no public source compiler or reload service.
Result CompileFallbackShader(const char* source, const char* profile, ShaderStage stage,
    DrawResources& resources, ShaderHandle& handle)
{
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT compiled = D3DCompile(source, std::strlen(source), "RendererFallback",
        nullptr, nullptr, "main", profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &bytecode, &errors);
    if (FAILED(compiled))
    {
        std::string message = "Cannot compile the Renderer resident fallback shader.";
        if (errors)
        {
            message.append(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        }

        return Fail(message);
    }

    ShaderDesc description;
    description.stage = stage;
    description.bytecode = std::span(static_cast<const std::byte*>(bytecode->GetBufferPointer()), bytecode->GetBufferSize());
    ResourceRegistrationResult<ShaderHandle> registered = resources.RegisterShader(description);
    if (!registered.result.IsSuccess())
    {
        return registered.result;
    }

    handle = registered.handle;
    return {};
}

}

Result DrawResources::InitializeFallbacks(GeometryRegistry& geometries, TextureRegistry& textures)
{
    // Eight local corners and outward-wound triangles form a conspicuous unit
    // cube. CullNone makes the emergency object readable under mirrored Worlds.
    const std::array<std::array<float, 3>, 8> corners{{
        {{-0.5f, -0.5f, -0.5f}}, {{0.5f, -0.5f, -0.5f}},
        {{0.5f, 0.5f, -0.5f}}, {{-0.5f, 0.5f, -0.5f}},
        {{-0.5f, -0.5f, 0.5f}}, {{0.5f, -0.5f, 0.5f}},
        {{0.5f, 0.5f, 0.5f}}, {{-0.5f, 0.5f, 0.5f}}
    }};
    std::array<MeshVertex, 8> vertices{};
    for (std::size_t index = 0; index < vertices.size(); ++index)
    {
        MeshVertex vertex;
        vertex.position.x = corners[index][0];
        vertex.position.y = corners[index][1];
        vertex.position.z = corners[index][2];
        vertex.uv.x = corners[index][0] + 0.5f;
        vertex.uv.y = corners[index][1] + 0.5f;
        vertex.color.x = 1;
        vertex.color.y = 1;
        vertex.color.z = 1;
        vertex.color.w = 1;
        vertices[index] = vertex;
    }

    const std::array<std::uint32_t, 36> indices{
        0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
        0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5
    };
    GeometryDesc geometryDescription;
    geometryDescription.vertices = vertices;
    geometryDescription.indices = indices;
    GeometryRegistrationResult geometryRegistration = geometries.Register(geometryDescription);
    if (!geometryRegistration.status.IsSuccess())
    {
        return geometryRegistration.status;
    }

    fallbackGeometry = geometryRegistration.handle;

    // A CPU-generated checker is independent from project paths and decoding.
    const std::array<std::uint8_t, 16> checker{
        255, 0, 255, 255, 0, 0, 0, 255,
        0, 0, 0, 255, 255, 0, 255, 255
    };
    TextureMipDesc mip;
    mip.rowPitch = 8;
    mip.pixels = std::as_bytes(std::span(checker));

    TextureDesc textureDescription;
    textureDescription.width = 2;
    textureDescription.height = 2;
    textureDescription.format = TextureFormat::RGBA8_sRGB;
    textureDescription.mips = std::span(&mip, 1);
    TextureRegistrationResult textureRegistration = textures.Register(textureDescription);
    if (!textureRegistration.status.IsSuccess())
    {
        return textureRegistration.status;
    }

    fallbackTexture = textureRegistration.handle;

    const char* vertexSource = R"(
cbuffer RendererView : register(b1)
{
    row_major float4x4 view;
    row_major float4x4 projection;
    float4 cameraPosition;
    row_major float4x4 shadowViewProjection;
    float4 shadowInformation;
};
cbuffer RendererObject : register(b4)
{
    row_major float4x4 world;
    row_major float4x4 normalMatrix;
    float4 tint;
};
struct Output
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};
Output main(float3 position : POSITION, float2 uv : TEXCOORD0)
{
    Output output;
    output.position = mul(mul(mul(float4(position, 1), world), view), projection);
    output.uv = uv;
    return output;
}
)";
    const char* pixelSource = R"(
Texture2D BaseTexture : register(t0);
SamplerState LinearWrap : register(s0);
float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return BaseTexture.Sample(LinearWrap, uv);
}
)";

    ShaderHandle vertexShader;
    Result prepared = CompileFallbackShader(vertexSource, "vs_5_0", ShaderStage::Vertex, *this, vertexShader);
    if (!prepared.IsSuccess())
    {
        return prepared;
    }

    ShaderHandle pixelShader;
    prepared = CompileFallbackShader(pixelSource, "ps_5_0", ShaderStage::Pixel, *this, pixelShader);
    if (!prepared.IsSuccess())
    {
        return prepared;
    }

    PipelineDesc pipelineDescription;
    pipelineDescription.vertexShader = vertexShader;
    pipelineDescription.pixelShader = pixelShader;
    pipelineDescription.depth = DepthMode::Disabled;
    pipelineDescription.cull = CullMode::None;
    ResourceRegistrationResult<PipelineHandle> pipelineRegistration = RegisterPipeline(pipelineDescription);
    if (!pipelineRegistration.result.IsSuccess())
    {
        return pipelineRegistration.result;
    }

    MaterialParameter textureParameter;
    textureParameter.name = "BaseTexture";
    textureParameter.value = fallbackTexture;

    MaterialDesc materialDescription;
    materialDescription.pipeline = pipelineRegistration.handle;
    materialDescription.parameters.push_back(textureParameter);
    // The resident Material can be used with the standard depth-only MeshVS
    // override. Its emergency Geometry must not inherit a missing Sprite's pivot.
    Vector4 identityQuad;
    identityQuad.x = 1;
    identityQuad.y = 1;
    MaterialParameter localQuad;
    localQuad.name = "LocalQuadTransform";
    localQuad.value = identityQuad;
    materialDescription.parameters.push_back(localQuad);
    ResourceRegistrationResult<MaterialHandle> materialRegistration = RegisterMaterial(materialDescription);
    if (!materialRegistration.result.IsSuccess())
    {
        return materialRegistration.result;
    }

    fallbackMaterial = materialRegistration.handle;
    return {};
}

}
