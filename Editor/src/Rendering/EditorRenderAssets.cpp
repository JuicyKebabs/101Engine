#include "Engine/Actor/ActorHierarchyActivity.h"
#include "EditorRenderAssets.h"

#include "RendererAssets/AssetFiles.h"
#include "Engine/Graphics/RenderingConversions.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Component/Collider.h"
#include "Engine/Core/Debug/Debug.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
rendering::Result Failure(const std::string& message)
{
    rendering::Result result;
    result.succeeded = false;
    result.message = "EditorRenderAssets: " + message;
    return result;
}

void AddParameter(rendering::MaterialDesc& material, const char* name, rendering::MaterialValue value)
{
    rendering::MaterialParameter parameter;
    parameter.name = name;
    parameter.value = std::move(value);
    material.parameters.push_back(std::move(parameter));
}
}

EditorRenderAssets::~EditorRenderAssets()
{
    // Order is logical dependencies first. Each request is independent from
    // physical GPU release, so a still-executing previous frame stays valid.
    for (const auto handle : m_maskMaterials)
    {
        const auto result = m_renderer.DestroyMaterial(handle);
        (void)result;
    }

    for (const auto handle : {m_outlineMaterial, m_colliderMaterial})
    {
        if (handle != rendering::MaterialHandle{})
        {
            const auto result = m_renderer.DestroyMaterial(handle);
            (void)result;
        }
    }

    for (const auto handle : m_debugGeometry)
    {
        if (handle != rendering::GeometryHandle{})
        {
            const auto result = m_renderer.DestroyGeometry(handle);
            (void)result;
        }
    }

    for (const auto handle : m_pipelines)
    {
        if (handle != rendering::PipelineHandle{})
        {
            const auto result = m_renderer.DestroyPipeline(handle);
            (void)result;
        }
    }

    for (const auto handle : m_shaders)
    {
        if (handle != rendering::ShaderHandle{})
        {
            const auto result = m_renderer.DestroyShader(handle);
            (void)result;
        }
    }

    for (const auto handle : {m_sceneColor, m_selectionMask})
    {
        if (handle != rendering::RenderTargetTextureHandle{})
        {
            const auto result = m_renderer.DestroyRenderTargetTexture(handle);
            (void)result;
        }
    }
}

rendering::Result EditorRenderAssets::RegisterShader(const std::string& source,
    rendering::ShaderStage stage, std::size_t index)
{
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    Microsoft::WRL::ComPtr<ID3DBlob> diagnostics;
    const char* profile = stage == rendering::ShaderStage::Vertex ? "vs_5_0" : "ps_5_0";
    const HRESULT compiled = D3DCompile(source.data(), source.size(), nullptr, nullptr,
        nullptr, "main", profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &bytecode, &diagnostics);

    if (FAILED(compiled))
    {
        const std::string message = diagnostics
            ? std::string(static_cast<const char*>(diagnostics->GetBufferPointer()), diagnostics->GetBufferSize())
            : "shader compilation failed";
        return Failure(message);
    }

    rendering::ShaderDesc description;
    description.stage = stage;
    description.bytecode = std::span(static_cast<const std::byte*>(bytecode->GetBufferPointer()), bytecode->GetBufferSize());
    const auto registration = m_renderer.RegisterShader(description);

    if (registration.result.IsSuccess())
    {
        m_shaders[index] = registration.handle;
    }

    return registration.result;
}

rendering::Result EditorRenderAssets::RegisterPipeline(std::size_t index, std::size_t vertexShader,
    std::size_t pixelShader, rendering::BlendMode blend, rendering::DepthMode depth,
    rendering::FillMode fill)
{
    rendering::PipelineDesc description;
    description.vertexShader = m_shaders[vertexShader];
    description.pixelShader = m_shaders[pixelShader];
    description.blend = blend;
    description.depth = depth;
    description.cull = rendering::CullMode::None;
    description.fill = fill;
    const auto registration = m_renderer.RegisterPipeline(description);

    if (registration.result.IsSuccess())
    {
        m_pipelines[index] = registration.handle;
    }

    return registration.result;
}

rendering::Result EditorRenderAssets::Initialize(const std::filesystem::path& shaderDirectory)
{
    // MeshVS is exactly the same compiled program as visible Engine objects.
    // Its b5 LocalQuadTransform therefore also positions the selection mask.
    const std::array<const char*, 2> files{"MeshVS.cso", "FullscreenVS.cso"};

    for (std::size_t index = 0; index < files.size(); ++index)
    {
        std::vector<std::byte> bytes;
        auto loaded = rendering::assets::LoadShader(shaderDirectory, files[index], bytes);

        if (!loaded.IsSuccess())
        {
            return loaded;
        }

        rendering::ShaderDesc description;
        description.stage = rendering::ShaderStage::Vertex;
        description.bytecode = bytes;
        const auto registration = m_renderer.RegisterShader(description);

        if (!registration.result.IsSuccess())
        {
            return registration.result;
        }

        m_shaders[index] = registration.handle;
    }

    const std::string pixelInput = R"HLSL(
struct Input
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float3 worldPosition : TEXCOORD1;
};
cbuffer MaterialParams : register(b5)
{
    float4 BaseColor;
    float4 UVTransform;
    float4 LocalQuadTransform;
};
)HLSL";
    const std::string solidMask = pixelInput + R"HLSL(
float4 main(Input input) : SV_TARGET { return float4(1, 1, 1, 1); }
)HLSL";
    const std::string alphaMask = pixelInput + R"HLSL(
Texture2D BaseTexture : register(t0);
SamplerState LinearWrap : register(s0);
float4 main(Input input) : SV_TARGET
{
    float alpha = BaseTexture.Sample(LinearWrap, input.uv * UVTransform.xy + UVTransform.zw).a;
    clip(alpha * BaseColor.a * input.color.a - 0.05f);
    return float4(1, 1, 1, 1);
}
)HLSL";
    const std::string outline = R"HLSL(
Texture2D SelectionMask : register(t0);
struct Input { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(Input input) : SV_TARGET
{
    uint width;
    uint height;
    SelectionMask.GetDimensions(width, height);
    int2 pixel = int2(input.position.xy);
    float center = SelectionMask.Load(int3(pixel, 0)).a;
    float nearby = 0;

    // Match a two-pixel outline while reading only numeric mask Alpha. An
    // emergency Geometry texture's RGB checker cannot introduce mask holes.
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            int2 samplePixel = clamp(pixel + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            nearby = max(nearby, SelectionMask.Load(int3(samplePixel, 0)).a);
        }
    }

    return float4(1.0f, 0.3185468f, 0.0331048f, saturate(nearby - center));
}
)HLSL";
    const std::string collider = pixelInput + R"HLSL(
float4 main(Input input) : SV_TARGET { return float4(0.1f, 1.0f, 0.1f, 1.0f); }
)HLSL";
    const std::array<std::string, 4> sources{solidMask, alphaMask, outline, collider};

    for (std::size_t index = 0; index < sources.size(); ++index)
    {
        auto compiled = RegisterShader(sources[index], rendering::ShaderStage::Pixel, index + 2);

        if (!compiled.IsSuccess())
        {
            return compiled;
        }
    }

    for (std::size_t index = 0; index < m_pipelines.size(); ++index)
    {
        const bool isOutline = index == 2;
        const bool isCollider = index == 3;
        auto registered = RegisterPipeline(index, isOutline ? 1 : 0, index + 2,
            isOutline ? rendering::BlendMode::Alpha : rendering::BlendMode::Opaque,
            isCollider ? rendering::DepthMode::ReadOnly : rendering::DepthMode::Disabled,
            isCollider ? rendering::FillMode::Wireframe : rendering::FillMode::Solid);

        if (!registered.IsSuccess())
        {
            return registered;
        }
    }

    rendering::RenderTargetTextureDesc target;
    target.width = m_width;
    target.height = m_height;
    target.format = rendering::TextureFormat::RGBA8_sRGB;
    auto color = m_renderer.CreateRenderTargetTexture(target);

    if (!color.result.IsSuccess())
    {
        return color.result;
    }

    m_sceneColor = color.handle;
    target.format = rendering::TextureFormat::RGBA8_UNorm;
    // Numeric coverage: Alpha zero means unselected. Match allocation advice
    // to the actual auxiliary Clear without changing the Outline algorithm.
    target.optimizedClearColor.fill(0.0f);
    auto mask = m_renderer.CreateRenderTargetTexture(target);

    if (!mask.result.IsSuccess())
    {
        return mask.result;
    }

    m_selectionMask = mask.handle;
    rendering::MaterialDesc outlineDescription;
    outlineDescription.pipeline = m_pipelines[2];
    AddParameter(outlineDescription, "SelectionMask", m_selectionMask);
    const auto outlineRegistration = m_renderer.RegisterMaterial(outlineDescription);

    if (!outlineRegistration.result.IsSuccess())
    {
        return outlineRegistration.result;
    }

    m_outlineMaterial = outlineRegistration.handle;
    rendering::MaterialDesc colliderDescription;
    colliderDescription.pipeline = m_pipelines[3];
    rendering::Vector4 identity;
    identity.x = 1;
    identity.y = 1;
    AddParameter(colliderDescription, "LocalQuadTransform", identity);
    const auto colliderRegistration = m_renderer.RegisterMaterial(colliderDescription);

    if (!colliderRegistration.result.IsSuccess())
    {
        return colliderRegistration.result;
    }

    m_colliderMaterial = colliderRegistration.handle;
    const std::array<DefaultMesh, 3> types{DefaultMesh::Cube, DefaultMesh::Sphere, DefaultMesh::Cylinder};

    for (std::size_t index = 0; index < types.size(); ++index)
    {
        const Model model = GetDefaultModel(types[index]);
        const Mesh& mesh = model.at(0);
        std::vector<rendering::MeshVertex> vertices;
        vertices.reserve(mesh.vertices.size());

        for (const Vertex& source : mesh.vertices)
        {
            rendering::MeshVertex vertex;
            vertex.position = EngineRendering::ToRenderer(source.position);
            vertex.normal = EngineRendering::ToRenderer(source.normal);
            vertex.uv.x = source.uv.x;
            vertex.uv.y = source.uv.y;
            vertex.tangent = EngineRendering::ToRenderer(source.tangent);
            vertex.color = EngineRendering::ToRenderer(source.color);
            vertices.push_back(vertex);
        }

        rendering::GeometryDesc description;
        description.vertices = vertices;
        description.indices = mesh.indices;
        const auto registered = m_renderer.RegisterGeometry(description);

        if (!registered.status.IsSuccess())
        {
            return registered.status;
        }

        m_debugGeometry[index] = registered.handle;
    }

    return {};
}

rendering::Result EditorRenderAssets::Resize(std::uint32_t width, std::uint32_t height)
{
    // Each Renderer resize replaces its own complete physical resource set.
    // This pair is not a transaction: Color can commit before Mask fails.
    // Publish dimensions only after both succeed. The host must propagate
    // failure to stop this frame and exit; it does not retry or draw the pair.
    auto color = m_renderer.ResizeRenderTargetTexture(m_sceneColor, width, height);

    if (!color.IsSuccess())
    {
        return color;
    }

    auto mask = m_renderer.ResizeRenderTargetTexture(m_selectionMask, width, height);

    if (!mask.IsSuccess())
    {
        return mask;
    }

    m_width = width;
    m_height = height;
    return {};
}

rendering::Result EditorRenderAssets::AppendMaskMaterial(const rendering::MaterialDesc& source,
    bool alphaMask, std::size_t slot, rendering::MaterialHandle& output)
{
    // Reserve the owning Handle slot before registration so allocation failure
    // cannot strand a successfully created Material registration.
    if (slot >= m_maskMaterials.size())
    {
        m_maskMaterials.resize(slot + 1);
    }

    rendering::MaterialDesc description = source;
    description.pipeline = m_pipelines[alphaMask ? 1 : 0];

    if (!alphaMask)
    {
        std::erase_if(description.parameters, [](const rendering::MaterialParameter& parameter)
        {
            return parameter.name == "BaseTexture";
        });
    }

    output = m_maskMaterials[slot];

    if (output != rendering::MaterialHandle{})
    {
        return m_renderer.UpdateMaterial(output, description);
    }

    const auto registered = m_renderer.RegisterMaterial(description);

    if (registered.result.IsSuccess())
    {
        m_maskMaterials[slot] = registered.handle;
        output = registered.handle;
    }

    return registered.result;
}

rendering::Result EditorRenderAssets::AppendSelection(rendering::FrameRenderData& frame, SceneBase& scene,
    const Guid& selectedActor, rendering::ViewId sourceView, bool canvasPreview)
{
    if (!selectedActor.IsValid())
    {
        return {};
    }

    rendering::AuxiliaryPass mask;
    mask.id = 10;
    mask.sourceViewId = sourceView;
    mask.name = "Editor.SelectionMask";
    mask.stage = canvasPreview ? rendering::AuxiliaryStage::AfterOverlay : rendering::AuxiliaryStage::BeforeDisplayTransform;
    mask.colorOutput = m_selectionMask;
    mask.colorLoad = rendering::AuxiliaryColorLoad::Clear;
    // Existing selection behavior includes occluded objects. No Scene Depth.
    mask.useSourceDepth = false;
    std::size_t materialSlot = 0;

    for (const auto& record : scene.GetRenderSystem()->GetExtractedObjects())
    {
        if (record.actorGuid != selectedActor || record.viewId != sourceView || !record.material)
        {
            continue;
        }

        rendering::MaterialHandle maskMaterial;
        auto prepared = AppendMaskMaterial(record.material->description, record.alphaMask, materialSlot++, maskMaterial);

        if (!prepared.IsSuccess())
        {
            return prepared;
        }

        rendering::AuxiliaryDraw draw;
        draw.objectIndex = record.objectIndex;
        draw.materialOverride = maskMaterial;
        mask.draws.push_back(draw);
    }

    if (mask.draws.empty())
    {
        return {};
    }

    rendering::RenderObject fullscreen;
    fullscreen.material = m_outlineMaterial;
    fullscreen.targetViews.push_back(11);
    rendering::ProceduralGeometry triangle;
    triangle.vertexCount = 3;
    fullscreen.procedural = triangle;
    const std::size_t outlineObject = frame.objects.size();
    frame.objects.push_back(fullscreen);

    rendering::AuxiliaryPass outline;
    outline.id = 11;
    outline.sourceViewId = sourceView;
    outline.name = "Editor.SelectionOutline";
    outline.stage = mask.stage;
    rendering::AuxiliaryDraw draw;
    draw.objectIndex = outlineObject;
    outline.draws.push_back(draw);
    frame.auxiliaryPasses.push_back(std::move(mask));
    frame.auxiliaryPasses.push_back(std::move(outline));
    return {};
}

void EditorRenderAssets::AppendColliderObject(rendering::FrameRenderData& frame,
    rendering::AuxiliaryPass& pass, rendering::GeometryHandle geometry, const Matrix4x4& world)
{
    rendering::RenderObject object;
    object.geometry = geometry;
    object.material = m_colliderMaterial;
    object.world = EngineRendering::ToRenderer(world);
    object.targetViews.push_back(pass.id);
    rendering::AuxiliaryDraw draw;
    draw.objectIndex = frame.objects.size();
    frame.objects.push_back(object);
    pass.draws.push_back(draw);
}

void EditorRenderAssets::AppendColliders(rendering::FrameRenderData& frame, SceneBase& scene,
    rendering::ViewId sourceView)
{
    rendering::AuxiliaryPass pass;
    pass.id = 12;
    pass.sourceViewId = sourceView;
    pass.name = "Editor.ColliderWireframe";
    pass.useSourceDepth = true;

    for (Actor* actor : scene.GetAllActors())
    {
        if (!IsActorHierarchyActive(actor))
        {
            continue;
        }

        for (Collider* collider : actor->GetComponentsByClass<Collider>())
        {
            if (!collider || !collider->isActive() || collider->GetType() == ColliderType::None)
            {
                continue;
            }

            collider->Flush();

            if (collider->GetType() == ColliderType::CAPSULE)
            {
                const CapsuleCollider& capsule = collider->GetCurrentCapsuleCollider();
                const float diameter = capsule.radius * 2;
                const auto transform = collider->GetWorldTransformCurrent();
                const Vector3 sphereScale(diameter, diameter, diameter);

                if (capsule.cylHeight <= 0)
                {
                    AppendColliderObject(frame, pass, m_debugGeometry[1], Matrix4x4::CreateTRS(
                        transform.position, Quaternion::Identity(), sphereScale));
                }
                else
                {
                    const Vector3 cylinderScale(diameter, capsule.cylHeight, diameter);
                    AppendColliderObject(frame, pass, m_debugGeometry[2], Matrix4x4::CreateTRS(
                        transform.position, transform.rotation, cylinderScale));
                    AppendColliderObject(frame, pass, m_debugGeometry[1], Matrix4x4::CreateTRS(
                        capsule.pointA, Quaternion::Identity(), sphereScale));
                    AppendColliderObject(frame, pass, m_debugGeometry[1], Matrix4x4::CreateTRS(
                        capsule.pointB, Quaternion::Identity(), sphereScale));
                }
            }
            else if (collider->GetType() == ColliderType::BOX || collider->GetType() == ColliderType::SPHERE)
            {
                const std::size_t geometryIndex = collider->GetType() == ColliderType::BOX ? 0 : 1;
                AppendColliderObject(frame, pass, m_debugGeometry[geometryIndex], collider->GetWorldMatrix());
            }
        }
    }

    if (!pass.draws.empty())
    {
        frame.auxiliaryPasses.push_back(std::move(pass));
    }
}
