// R7 implementation guide: section 4.3.
// Public draw/material vocabulary shared by frame submission and reflected shader inputs.
// Material sampling accepts either texture handle type without merging their owners.
// Reflection and diagnostics describe the final draw contract; shader reload remains deferred.

#pragma once
#include "Math.h"
#include "ResourceHandle.h"
#include "Result.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

//------------------------------------------------------------------------------------------------
// DrawTypes.h
// Publishing types for rendering API for drawing a single object with a material and a pipeline.
//------------------------------------------------------------------------------------------------

namespace rendering
{

//----------------------------------------------------------------
// Shader description
// Constructed from compiled shader by reflection API.
//----------------------------------------------------------------

enum class ShaderStage
{
	Vertex,
	Pixel
};

enum class ShaderStages : std::uint8_t
{
	None = 0,
	Vertex = 1,
	Pixel = 2
};

constexpr ShaderStages operator|(ShaderStages a, ShaderStages b) noexcept
{
	return static_cast<ShaderStages>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

// Concrete declaration of shader variable types.
// Do not change the order of this enum, because it is used in shader reflection and material parameter value type.
enum class ValueType
{
	Float,
	Float2,
	Float3,
	Float4,
	Int,
	Int2,
	Int3,
	Int4,
	UInt,
	UInt2,
	UInt3,
	UInt4,
	Bool,
	Matrix4x4,
	Texture2D
};

// Material parameter value type, which can be one of the following types:
// Both texture handle alternatives represent HLSL Texture2D when sampled.
// The alternative still selects a different CPU owner and different mutation API.
// Adding this public alternative requires clients to rebuild against the matching header/DLL.
using MaterialValue = 
	std::variant<float, Vector2, Vector3, Vector4, std::int32_t, std::array<std::int32_t, 2>,
		std::array<std::int32_t, 3>, std::array<std::int32_t, 4>, std::uint32_t,
		std::array<std::uint32_t, 2>, std::array<std::uint32_t, 3>,
		std::array<std::uint32_t, 4>, bool, Matrix4x4, TextureHandle, RenderTargetTextureHandle>;

struct MaterialParameter
{
	std::string name;
	MaterialValue value; // Store value without fixing the type, because the type is determined by the shader reflection.
};

struct ShaderVariable
{
	std::string name;					// Name of the variable in the shader.
	ValueType type = ValueType::Float;	// Concrete type of the variable.
	std::uint32_t offset = 0;			// Offset in bytes from the start of the constant buffer.
	std::uint32_t size = 0;				// Total size in bytes of the variable, including padding.
	bool rowMajor = false;				// For matrix type, true if row-major, false if column-major.
	bool used = false;					// If the shader use this variable.
};

struct ShaderConstantBuffer
{
	std::string name;						// Name of the constant buffer in the shader.
	std::uint32_t binding = 0;				// Register number (e.g., b0, b1, etc.)
	std::uint32_t size = 0;					// Total size in bytes of the constant buffer, including padding.
	std::vector<ShaderVariable> variables;	// List of variables in the constant buffer.
};

struct ShaderResourceBinding
{
	std::string name;							// Name of the resource binding in the shader.
	std::uint32_t binding = 0;					// Register number (e.g., t0, s0, etc.)
	bool sampler = false;						// True if the resource is a sampler, false if it is a texture.
	ShaderStages stages = ShaderStages::None;	// Shader stages that use this resource binding.
};

struct ShaderVertexInput
{
	std::string semantic;
	std::uint32_t index = 0;
	std::uint32_t components = 0;
};

struct ShaderMetadata
{
	ShaderStage stage = ShaderStage::Vertex;
	std::uint64_t revision = 1;
	std::vector<ShaderConstantBuffer> constants;
	std::vector<ShaderResourceBinding> resources;
	std::vector<ShaderVertexInput> vertexInputs;
	std::uint32_t colorOutputs = 0;
	// Reflection records the system-provided scalar separately from user vertex attributes.
	// It must not create a VB InputElement for SV_VertexID.
	bool usesVertexId = false;
};

struct ShaderDesc
{
	ShaderStage stage = ShaderStage::Vertex;
	std::span<const std::byte> bytecode;
};

//----------------------------------------------------------------
// Pipeline description
// Use to create a PipelineState object.
//----------------------------------------------------------------

enum class BlendMode
{
	Opaque,
	Alpha
};

enum class DepthMode
{
	Disabled,
	ReadOnly,
	ReadWrite
};

enum class CullMode
{
	None,
	Back,
	Front
};

struct PipelineDesc
{
	ShaderHandle vertexShader;
	ShaderHandle pixelShader;
	BlendMode blend = BlendMode::Opaque;
	DepthMode depth = DepthMode::ReadWrite;
	CullMode cull = CullMode::Back;
};

//----------------------------------------------------------------
// Material description
// One on one relation with RenderObject in FrameRenderData.
//----------------------------------------------------------------

struct MaterialDesc
{
	PipelineHandle pipeline;
	std::vector<MaterialParameter> parameters;
};

//----------------------------------------------------------------
// Draw operation related types
//----------------------------------------------------------------

// Override settings for drawing a single object.
struct DrawSettings
{
	std::optional<MaterialHandle> materialOverride;
	std::optional<PipelineHandle> pipelineOverride;
};

struct DrawDiagnostic
{
	std::size_t objectIndex = 0;
	std::string passName = "Scene";
	std::string stage;
	std::string message;
	GeometryHandle geometry;
	MaterialHandle material;
	PipelineHandle pipeline;
	ShaderHandle vertexShader;
	ShaderHandle pixelShader;
	std::uint64_t materialRevision = 0;
	std::uint64_t geometryRevision = 0;
	std::uint64_t vertexRevision = 0;
	std::uint64_t pixelRevision = 0;
	// The same source Object can fail in only one View after an override or membership choice.
	// Keep View identity with the Object diagnostic so those cases can be distinguished.
	std::uint32_t viewId = 0;
};

//-----------------------------------------------------------------------------
// Resource registration result
// Registration API returns a result and a handle for the registered resource.
//-----------------------------------------------------------------------------

template <class Handle>
struct [[nodiscard]] ResourceRegistrationResult
{
	Result result;
	Handle handle;
};

}