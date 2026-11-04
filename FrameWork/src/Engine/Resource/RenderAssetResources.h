#pragma once

#include "Renderer/Renderer.h"
#include "Engine/Core/Math/Math.h"
#include <array>
#include <filesystem>

//----------------------------------------------------------------------------------------------------
// RenderAssetResources
// A small set of standard resources that the Engine uses for its own rendering.
// These resources are not owned by any Scene, and they are not released until the Engine shuts down.
//----------------------------------------------------------------------------------------------------

// App-lifetime standard resources, outside all Scene-owned resource sets.
// Renderer owns every GPU object; this owner only retains typed registrations.
class RenderAssetResources
{
public:
	bool Initialize(rendering::Renderer& renderer, const std::filesystem::path& absoluteShaderDirectory);
	void Finalize() noexcept;
	~RenderAssetResources();

	rendering::GeometryHandle Quad() const noexcept { return m_quad; }
	rendering::TextureHandle White() const noexcept { return m_white; }
	rendering::PipelineHandle MeshPipeline(bool screen) const noexcept;
	rendering::PipelineHandle TexturedPipeline(bool screen) const noexcept;
	rendering::MaterialDesc MakeMaterial(
		bool mesh,
		bool screen,
		rendering::TextureHandle texture,
		const Vector2& uvScale,
		const Vector2& uvOffset,
		const Vector2& pivot,
		bool flipX,
		bool flipY) const;

private:
	rendering::Renderer* m_renderer = nullptr;

	std::array<rendering::ShaderHandle, 3> m_shaders{};
	std::array<rendering::PipelineHandle, 4> m_pipelines{};

	rendering::GeometryHandle m_quad;	// Standard quad geometry
	rendering::TextureHandle m_white;	// Default white texture (The texture for missing is prepared in Renderer)
};
