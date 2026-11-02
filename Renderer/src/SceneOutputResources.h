#pragma once

#include "DrawSystem.h"

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// SceneOutputResources
// This manages the GPU resources for a single Scene output identity.
// The word "Scene" in this context means ViewKind::Scene, not the entire Renderer or a host's world.
// A rendering process to the scene output requires the intermediate resources such as a private HDR color image and a shadow depth image.
// SceneOutputImages and SceneOutputResources manages the working resources of such kind of resources (includeing pipeline and material) instead of the impl of Renderer.
// When the first usage of a new output identity is detected, the resources are created and cached for later usage,
// and released when the output identity is no longer valid for the current frame after GPU completed usage.
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace rendering::detail
{

// One physical working set per live output identity. Frame-local View IDs do
// not accumulate persistent images when a host changes its View numbering.
struct SceneOutputImages
{
	ViewOutput output;									// Handle for output resource (Window or RenderTargetTexture).
	RenderTargetTextureHandle hdrColor;					// Handle for storing the private HDR color image for this output (used after conversion to sRGB for display).
	MaterialHandle displayMaterial;						// Handle for the material used to display the HDR color image.
	float exposure = -1;								// Last exposure value used to create the display material; if the exposure changes, the display material needs to be recreated.
	std::unique_ptr<backend::DepthTexture> shadowDepth; // One shadow depth per output.
};

class SceneOutputResources
{
public:
	Result Initialize(backend::Backend& backend, DrawResources& draws);

	Result Prepare(
		const RenderView& view, backend::Backend& backend,
		RenderTargetTextureRegistry& targets, DrawResources& draws,
		SceneOutputImages*& images, Result& shadowPreparation, bool needsShadow);

	Result PruneOutputs(backend::Backend& backend, RenderTargetTextureRegistry& targets, DrawResources& draws);

	backend::DepthTexture& DisabledShadow() noexcept
	{
		return m_disabledShadow;
	}
	PipelineHandle ShadowPipeline() const noexcept
	{
		return m_shadowPipeline;
	}

private:
	std::vector<std::unique_ptr<SceneOutputImages>> m_outputs;	// Cache of output resources, one per unique output identity.
	backend::DepthTexture m_disabledShadow;						// 1x1 depth texture binded when shadows are disabled.
	PipelineHandle m_displayPipeline;							// Handle for the pipeline used to display the HDR color image on the output.
	PipelineHandle m_shadowPipeline;							// Handle for the pipeline used to render shadows.
};

} // namespace rendering::detail