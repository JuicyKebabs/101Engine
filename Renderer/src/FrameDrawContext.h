// R7 implementation guide: section 4.29.
// One Render-long borrowed gateway to frame data, registries, Graph bindings and Backend.
// Original Object indices are stable throughout declaration and recording.
// Current View information is a short recording context, not a shared mutable Camera resource.

#pragma once

#include "DrawSystem.h"
#include "DrawPlan.h"
#include "Resource/RenderTargetTextureRegistry.h"
#include "Graph/FrameResourceBindings.h"

namespace rendering::detail
{

class SceneOutputResources;
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
struct FrameDrawServices
{
	DrawResources* draws = nullptr;
	GeometryRegistry* geometries = nullptr;
	TextureRegistry* textures = nullptr;
	backend::Backend* backend = nullptr;
	backend::DepthTexture* depth = nullptr;
	graph::FrameResourceBindings* bindings = nullptr;
	RenderTargetTextureRegistry* targets = nullptr;
	SceneOutputResources* sceneResources = nullptr;
};

class FrameDrawContext
{
public:
	// Internal value query for planning compatible borrowed attachments.
	Result GetViewExtent(const RenderView& view, std::uint32_t& width, std::uint32_t& height);

	FrameDrawContext(
		const FrameRenderData& frame,
		const FrameDrawServices& services,
		std::vector<DrawDiagnostic>& diagnostics,
		std::size_t& drawnObjects) noexcept;

	Result PrepareSceneImages(const RenderView& view, SceneOutputImages*& images, Result& shadowPreparation);
	backend::DepthTexture& DisabledShadow() noexcept;
	backend::DepthTexture* SceneDepth(SceneOutputImages& images) noexcept;
	void ReportViewFailure(ViewId viewId, const std::string& stage, const std::string& message);
	PipelineHandle ShadowPipeline() const noexcept;

	// Explicit auxiliary indices bypass automatic View membership selection.
	// They still use the identical resource validation and fallback resolution.
	bool SelectExplicitDraw(std::size_t index, const DrawSettings& settings, const PassRecordingInputs& pass);

	// Selection stores original Object indices, without copying World/Tint or packed Material data.
	Result SelectAvailableDraws(
		const DrawSettings& settings,
		RenderList& objectIndices,
		const PassRecordingInputs& pass);
	
	Result ImportDepth(backend::DepthTexture& depth, graph::GraphTextureHandle& texture) { return m_bindings.ImportDepth(depth, texture); }
	
	bool UsesAlphaBlending(std::size_t objectIndex, const DrawSettings& settings, const PassRecordingInputs& pass);
	
	Result CollectUploadTargets(
		std::span<const PlannedDraw> draws,
		const PassRecordingInputs& pass,
		std::vector<GeometryUploadTarget>& geometries,
		std::vector<TextureUploadTarget>& textures);

	Result CheckPreparedDraws(std::vector<PlannedDraw>& draws, const PassRecordingInputs& pass);
	Result PrepareViewTargets(const RenderView& view, graph::ViewTargets& targets);
	
	// Use identical settings for declaration and recording in this mutation-free Render scope.
	Result DeclareDrawResources(
		graph::RenderPassBuilder& builder,
		std::span<const PlannedDraw> draws
		, const PassRecordingInputs& pass);

	Result RecordPass(
		graph::RenderPassContext& context,
		const PassRecordingInputs& pass,
		std::span<const PlannedDraw> draws);

	Result CheckOutputReadOrder(
		std::span<const PlannedDraw> draws,
		const PassRecordingInputs& pass,
		const std::vector<RenderTargetTextureHandle>& producers);

	Result EstimateRequirements(
		std::span<const PlannedDraw> draws,
		const PassRecordingInputs& pass,
		std::uint64_t& constantBytes,
		std::uint64_t& descriptors);

	Result ResolveSample(
		const MaterialDesc& material,
		const ShaderResourceBinding& requirement,
		SampledTexture& sample,
		const PassRecordingInputs& pass);

	Result ResolveObjectInputs(
		std::size_t objectIndex, const DrawSettings& settings, DrawInputs& inputs,
		DrawDiagnostic& diagnostic, const PassRecordingInputs& pass);

	void AddDiagnostic(
		DrawDiagnostic diagnostic,
		const std::string& stage,
		const std::string& message,
		const PassRecordingInputs& pass);

	bool AreResourcesPrepared(const DrawInputs& inputs, const PassRecordingInputs& pass);

	const FrameRenderData& Frame() const noexcept { return m_frame; }
	backend::Backend& Backend() noexcept { return m_backend; }
	graph::FrameResourceBindings& Bindings() noexcept { return m_bindings; }

private:
	SceneOutputResources* m_sceneResources = nullptr;
	const FrameRenderData& m_frame;
	DrawResources& m_resources;
	backend::Backend& m_backend;
	backend::DepthTexture& m_depth;
	graph::FrameResourceBindings& m_bindings;
	std::vector<DrawDiagnostic>& m_diagnostics;
	std::size_t& m_drawnObjects;
};

} // namespace rendering::detail
