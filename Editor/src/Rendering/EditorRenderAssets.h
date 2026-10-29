#pragma once

#include "Renderer/Renderer.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Graphics/RenderData.h"
#include <array>
#include <filesystem>

class SceneBase;

// These registrations belong to the Editor, outside every document/Play Scene.
// The Renderer still owns their physical allocations and Fence retention.
class EditorRenderAssets
{
public:
    explicit EditorRenderAssets(rendering::Renderer& renderer) : m_renderer(renderer) {}
    ~EditorRenderAssets();
    rendering::Result Initialize(const std::filesystem::path& shaderDirectory);
    rendering::Result Resize(std::uint32_t width, std::uint32_t height);
    rendering::Result AppendSelection(rendering::FrameRenderData& frame, SceneBase& scene,
        const Guid& selectedActor, rendering::ViewId sourceView, bool canvasPreview);
    void AppendColliders(rendering::FrameRenderData& frame, SceneBase& scene, rendering::ViewId sourceView);

    rendering::RenderTargetTextureHandle SceneColor() const { return m_sceneColor; }
    std::uint32_t Width() const { return m_width; }
    std::uint32_t Height() const { return m_height; }

private:
    rendering::Result RegisterShader(const std::string& source, rendering::ShaderStage stage,
        std::size_t index);
    rendering::Result RegisterPipeline(std::size_t index, std::size_t vertexShader,
        std::size_t pixelShader, rendering::BlendMode blend, rendering::DepthMode depth,
        rendering::FillMode fill);
    rendering::Result AppendMaskMaterial(const rendering::MaterialDesc& source,
        bool alphaMask, std::size_t slot, rendering::MaterialHandle& output);
    void AppendColliderObject(rendering::FrameRenderData& frame, rendering::AuxiliaryPass& pass,
        rendering::GeometryHandle geometry, const Matrix4x4& world);

    rendering::Renderer& m_renderer;
    rendering::RenderTargetTextureHandle m_sceneColor;
    rendering::RenderTargetTextureHandle m_selectionMask;
    std::uint32_t m_width = 1280;
    std::uint32_t m_height = 720;
    std::array<rendering::ShaderHandle, 6> m_shaders{};
    std::array<rendering::PipelineHandle, 4> m_pipelines{};
    std::array<rendering::GeometryHandle, 3> m_debugGeometry{};
    std::vector<rendering::MaterialHandle> m_maskMaterials;
    rendering::MaterialHandle m_outlineMaterial;
    rendering::MaterialHandle m_colliderMaterial;
};
