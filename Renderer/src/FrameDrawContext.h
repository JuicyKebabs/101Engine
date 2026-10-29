// R7 implementation guide: section 4.29.
// One Render-long borrowed gateway to frame data, registries, Graph bindings and Backend.
// Original Object indices are stable throughout declaration and recording.
// Current View information is a short recording context, not a shared mutable Camera resource.

#pragma once

#include "DrawSystem.h"
#include "Resource/RenderTargetTextureRegistry.h"
#include "Graph/FrameResourceBindings.h"

namespace rendering::detail
{

class SceneFrameResources;
struct SceneOutputImages;

struct GeometryUploadTarget
{
	GeometryHandle handle;
	GeometryContent* content = nullptr;
};

struct TextureUploadTarget
{
	TextureHandle handle;
	TextureContent* content = nullptr;
};

// Only borrowed services for one Render call. Resource mutations are prohibited
// throughout this scope, so repeated lookups select the same current content.
class FrameDrawContext
{
public:
    // Internal value query for planning compatible borrowed attachments.
    Result GetViewExtent(const RenderView& view, std::uint32_t& width, std::uint32_t& height);
	FrameDrawContext(
		const FrameRenderData& frame,
		DrawResources& resources,
		GeometryRegistry& geometries,
		TextureRegistry& textures,
		backend::Backend& backend,
		backend::DepthTexture& depth,
		graph::FrameResourceBindings& bindings,
		std::vector<DrawDiagnostic>& diagnostics,
		std::size_t& drawnObjects,
		RenderTargetTextureRegistry* targets = nullptr, SceneFrameResources* sceneResources = nullptr) noexcept;

    Result PrepareSceneImages(const RenderView& view, SceneOutputImages*& images, Result& shadowPreparation);
    backend::DepthTexture& DisabledShadow() noexcept;
    backend::DepthTexture* SceneDepth(SceneOutputImages& images) noexcept;
    void ReportViewFailure(ViewId viewId, const std::string& stage, const std::string& message);
    PipelineHandle ShadowPipeline() const noexcept;
    void SetRecordingExtent(std::uint32_t width, std::uint32_t height) noexcept
    {
        m_recordingWidth = width;
        m_recordingHeight = height;
    }

    // Explicit auxiliary indices bypass automatic View membership selection.
    // They still use the identical resource validation and fallback resolution.
    bool SelectExplicitDraw(std::size_t index, const DrawSettings& settings);

	// Selection stores original Object indices, without copying World/Tint or packed Material data.
	Result SelectAvailableDraws(const DrawSettings& settings, RenderList& objectIndices, const RenderView* view = nullptr);
    void SetActiveView(ViewId id) noexcept { m_activeViewId = id; }
    void SetSceneInputs(const RenderView* billboardSource, backend::DepthTexture* shadow,
        const Matrix4x4& shadowViewProjection, bool enabled) noexcept;
    Result ImportDepth(backend::DepthTexture& depth, graph::GraphTextureHandle& texture)
    {
        return m_bindings.ImportDepth(depth, texture);
    }
	bool UsesAlphaBlending(std::size_t objectIndex, const DrawSettings& settings);
	Result CollectUploadTargets(
		const RenderList& objectIndices,
		const DrawSettings& settings,
		std::vector<GeometryUploadTarget>& geometries,
		std::vector<TextureUploadTarget>& textures);
	Result CheckPreparedDraws(RenderList& objectIndices, const DrawSettings& settings);
	Result PrepareViewTargets(const RenderView& view, graph::ViewTargets& targets);
	// Use identical settings for declaration and recording in this mutation-free Render scope.
	Result DeclareDrawResources(graph::RenderPassBuilder& builder, const RenderList& objectIndices, const DrawSettings& settings);
	Result RecordRenderList(
		graph::RenderPassContext& context,
		const RenderView& view,
		const RenderList& objectIndices,
		const DrawSettings& settings);

	Result CheckOutputReadOrder(const RenderList& indices, const DrawSettings& settings,
        const FrameRenderData& frame, const std::vector<RenderTargetTextureHandle>& producers);
    Result EstimateRequirements(const RenderList& indices, const DrawSettings& settings,
        std::uint64_t& constantBytes, std::uint64_t& descriptors, std::uint64_t& retained);

private:
    Result ResolveSample(const MaterialDesc& material, const ShaderResourceBinding& requirement, SampledTexture& sample);
	Result ResolveObjectInputs(std::size_t objectIndex, const DrawSettings& settings, DrawInputs& inputs, DrawDiagnostic& diagnostic);
	void AddDiagnostic(DrawDiagnostic diagnostic, const std::string& stage, const std::string& message);
	bool AreResourcesPrepared(const DrawInputs& inputs);

    SceneFrameResources* m_sceneResources = nullptr;
    std::uint32_t m_recordingWidth = 0;
    std::uint32_t m_recordingHeight = 0;
    const RenderView* m_billboardSource = nullptr;
    backend::DepthTexture* m_shadowTexture = nullptr;
    Matrix4x4 m_shadowViewProjection = Matrix4x4::Identity();
    bool m_shadowEnabled = false;
    RenderTargetTextureRegistry* m_renderTargets = nullptr;
    Viewport m_activeViewport;
    std::uint32_t m_activeTargetWidth = 0;
    std::uint32_t m_activeTargetHeight = 0;
    ViewId m_activeViewId = 0;
	const FrameRenderData& m_frame;
	DrawResources& m_resources;
	GeometryRegistry& m_geometries;
	TextureRegistry& m_textures;
	backend::Backend& m_backend;
	backend::DepthTexture& m_depth;
	graph::FrameResourceBindings& m_bindings;
	std::vector<DrawDiagnostic>& m_diagnostics;
	std::size_t& m_drawnObjects;

	friend class DrawResources;
};

}
