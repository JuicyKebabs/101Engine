#pragma once

#include "DrawSystem.h"

namespace rendering::detail
{

// One physical working set per live output identity. Frame-local View IDs do
// not accumulate persistent images when a host changes its View numbering.
struct SceneOutputImages
{
    ViewOutput output;
    RenderTargetTextureHandle hdrColor;
    MaterialHandle displayMaterial;
    float exposure = -1;
    std::unique_ptr<backend::DepthTexture> shadowDepth;
};

class SceneFrameResources
{
public:
    Result Initialize(backend::Backend& backend, DrawResources& draws);
    Result Prepare(const RenderView& view, backend::Backend& backend,
        RenderTargetTextureRegistry& targets, DrawResources& draws,
        SceneOutputImages*& images, Result& shadowPreparation, bool needsShadow);
    Result Prune(backend::Backend& backend, RenderTargetTextureRegistry& targets, DrawResources& draws);

    backend::DepthTexture& DisabledShadow() noexcept { return m_disabledShadow; }
    PipelineHandle ShadowPipeline() const noexcept { return m_shadowPipeline; }

private:
    // Registry handles protect GPU lifetime through the existing retired/Fence
    // machinery. The cache owns the right to Destroy these private registrations.
    std::vector<std::unique_ptr<SceneOutputImages>> m_outputs;
    backend::DepthTexture m_disabledShadow;
    PipelineHandle m_displayPipeline;
    PipelineHandle m_shadowPipeline;
};

}
