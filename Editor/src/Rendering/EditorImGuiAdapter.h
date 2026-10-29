// Editor-owned translation of ImGui CPU values to Renderer registrations.
// One host Window; no native descriptors or command callbacks cross this boundary.
#pragma once

#include "Renderer/Renderer.h"
#include "imgui.h"

#include <unordered_map>

namespace rendering::editor
{

// Handles fonts, texture updates, DrawList offsets and per-command scissors.
// It owns Renderer registrations. It never borrows ImGui arrays after Append.
// Call Initialize once per adapter. Call Shutdown before Renderer finalization,
// including when Initialize returns a failure after partial registration.
class EditorImGuiAdapter
{
public:
	explicit EditorImGuiAdapter(Renderer& renderer) : m_renderer(renderer) {}

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
