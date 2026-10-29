// R7 implementation guide: section 4.38.
// Small client-side ImGui integration probe; the Renderer core has no ImGui dependency.
// Owns registrations it creates, but only borrows externally supplied Scene output handles.
// Initialize once; call Shutdown even after partial initialization and before Renderer finalization.

#pragma once

#include "Renderer/Renderer.h"
#include "imgui.h"

#include <unordered_map>

namespace rendering::probes
{

// A small integration probe, not a replacement for the complete R9 adapter.
// It owns Renderer registrations. It never borrows ImGui arrays after Append.
// Call Initialize once per adapter. Call Shutdown before Renderer finalization,
// including when Initialize returns a failure after partial registration.
class MinimalImGuiAdapter
{
public:
	explicit MinimalImGuiAdapter(Renderer& renderer) : m_renderer(renderer) {}

	Result Initialize();
	Result Shutdown();
	Result ProcessTexture(ImTextureData& texture);
	Result RegisterImage(TextureHandle texture, ImTextureID& identifier);
	Result RegisterImage(RenderTargetTextureHandle texture, ImTextureID& identifier);
	Result Append(const ImDrawData& drawData, ViewId view, FrameRenderData& frame);

private:
	struct ImageEntry
	{
		MaterialHandle material;
		TextureHandle uploadedTexture;
		bool ownsTexture = false;
	};

	Result AddImage(MaterialValue source, TextureHandle uploadedTexture, bool ownsTexture, ImTextureID& identifier);
	Result CopyTexturePixels(const ImTextureData& texture, std::vector<std::byte>& pixels) const;
	Result StoreDrawList(const ImDrawList& drawList, const ImDrawData& drawData, std::size_t listIndex);

	Renderer& m_renderer;
	ShaderHandle m_vertexShader;
	ShaderHandle m_pixelShader;
	PipelineHandle m_pipeline;
	std::unordered_map<ImTextureID, ImageEntry> m_images;
	std::vector<GeometryHandle> m_geometries;
	ImTextureID m_nextIdentifier = 1;
};

}
