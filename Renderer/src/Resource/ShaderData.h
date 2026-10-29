#pragma once

#include "Renderer/DrawTypes.h"

#include <vector>

//------------------------------------------------------------------------------------
// ShaderData.h
// Reflect shader resource and manage the shader content and metadata.
//------------------------------------------------------------------------------------

namespace rendering::detail
{

// Stores the compiled shader bytecode and metadata for a shader resource.
struct ShaderContent
{
	std::uint64_t revision = 1;				// Incremented on each update to the shader content.
	std::uint64_t lastDrawFenceValue = 0;	// Same Graphics Queue timeline. 
	std::vector<std::byte> bytecode;		// Compiled shader bytecode.
	ShaderMetadata metadata;				// Metadata describing the shader's resources, constants, and inputs.
};

// Construct ShaderContent by reflecting the given shader bytecode and populating the metadata.
Result ReflectShader(const ShaderDesc& desc, ShaderContent& output);

// Create the single metadata for pipleine resource by merging the vertex and pixel shader metadata.
// Combines the duplicated constant buffers and resources.
Result MergeMetadata(const ShaderMetadata& vertex, const ShaderMetadata* pixel, ShaderMetadata& output);

// Validate the given material description.
Result ValidateMaterial(const MaterialDesc& desc);

// Convert the abstract type of the material value to the concrete ValueType enum.
ValueType TypeOf(const MaterialValue& value) noexcept;

// Pack the material description into a binary format suitable for GPU consumption, based on the provided layout.
// Connects the byte layout of the shader constant buffer with the material concrete values.
// This makes the final output binary data which has the material values with the correct binary layout for the shader constant buffer.
Result PackMaterial(const ShaderConstantBuffer& layout, const MaterialDesc& desc, std::vector<std::byte>& output);

}