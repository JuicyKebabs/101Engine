#pragma once

#include "Renderer/Renderer.h"
#include "RendererAssets/AssetFiles.h"

#include <filesystem>

inline rendering::ResourceRegistrationResult<rendering::ShaderHandle> LoadAndRegisterShader(
	rendering::Renderer& renderer,
	const std::filesystem::path& shaderDirectory,
	const std::string& source,
	rendering::ShaderStage stage)
{
	rendering::ResourceRegistrationResult<rendering::ShaderHandle> result;

	// Load compiled shader bytecode from disk
	std::vector<std::byte> bytes;
	auto loaded = rendering::assets::LoadShader(shaderDirectory, source, bytes);

	if (!loaded.IsSuccess())
	{
		result.result = loaded;
		return result;
	}

	// Register shader with the renderer
	rendering::ShaderDesc description;
	description.stage = stage;
	description.bytecode = bytes;

	return renderer.RegisterShader(description);

}