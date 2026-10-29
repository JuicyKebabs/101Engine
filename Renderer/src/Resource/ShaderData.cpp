// R7 implementation guide: section 4.20.
// Reflection is the contract between accepted Shader bytecode and common Draw inputs.
// SV_VertexID is supplied by the pipeline and is not a user Vertex Buffer input.
// The existing corrected MergeMetadata binding comparison is preserved, not reimplemented here.

#include "ShaderData.h"
#include "Failure.h"
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace rendering::detail
{

static_assert(sizeof(Vector2) == 8 && sizeof(Vector3) == 12 && sizeof(Vector4) == 16 && sizeof(Matrix4x4) == 64, "ShaderData: unexpected size of math types.");

ValueType TypeOf(const MaterialValue& value) noexcept
{
	// Both resource kinds satisfy one HLSL Texture2D input. Their ownership stays separate.
	if (std::holds_alternative<RenderTargetTextureHandle>(value))
	{
		return ValueType::Texture2D;
	}

	static_assert(std::variant_size_v<MaterialValue> == 16);
	return static_cast<ValueType>(value.index());
}

namespace
{

// Check if the given shader type is a supported variable type and convert it to ValueType.
Result VariableType(const D3D12_SHADER_TYPE_DESC& type, ShaderVariable& output)
{
	// Array and structure types are not supported now.
	if (type.Elements || type.Members)
	{
		return { false, "VariableType: arrays and structures are not supported." };
	}

	// Matrix4x4 (float4x4)
	if (type.Type == D3D_SVT_FLOAT && type.Rows == 4 && type.Columns == 4 &&
		(type.Class == D3D_SVC_MATRIX_ROWS || type.Class == D3D_SVC_MATRIX_COLUMNS))
	{
		output.type = ValueType::Matrix4x4;
		output.rowMajor = type.Class == D3D_SVC_MATRIX_ROWS;	// For converting matrix to byte array.
		return {};
	}

	if (type.Rows != 1 || type.Columns < 1 || type.Columns > 4)
	{
		return { false, "VariableType: unsupported variable rows/columns." };
	}

	if (type.Class != D3D_SVC_SCALAR && type.Class != D3D_SVC_VECTOR)
	{
		return { false, "VariableType: unsupported variable class." };
	}

	unsigned base = 0;

	// Determine the ValueType based on the scalar type and number of columns.
	// Declaration order in ValueType enum is important for this mapping to work correctly. 
	// Float, Float2, Float3, Float4, Int, Int2, Int3, Int4, UInt, UInt2, UInt3, UInt4, Bool
	switch (type.Type)
	{
	case D3D_SVT_FLOAT:
		base = 0;
		break;

	case D3D_SVT_INT:
		base = 4;
		break;

	case D3D_SVT_UINT:
		base = 8;
		break;

	case D3D_SVT_BOOL:
		if (type.Columns != 1)
		{
			return { false, "only scalar bool is supported" };
		}

		output.type = ValueType::Bool;
		return {};

	default:
		return { false, "unsupported scalar precision/type" };
	}

	output.type = static_cast<ValueType>(base + type.Columns - 1);
	return {};
}

// Check if the given shader constant buffer matches the expected common schema for renderer buffers.
// b0: RendererFrame, b1: RendererView, b2: RendererLight, b3: RendererPass, b4: RendererObject
// b5: Free for user-defined constant buffers.
Result CommonSchema(const ShaderConstantBuffer& buffer)
{
	struct Field
	{
		const char* name;
		ValueType type;
		UINT offset;
	};

	const std::vector<std::vector<Field>> fields{ { { "time", ValueType::Float, 0 } },
													{ { "view", ValueType::Matrix4x4, 0 },
													{ "projection", ValueType::Matrix4x4, 64 },
													{ "cameraPosition", ValueType::Float4, 128 },
                                                    { "shadowViewProjection", ValueType::Matrix4x4, 144 },
                                                    { "shadowInformation", ValueType::Float4, 208 } },
													{ { "lightDirection", ValueType::Float4, 0 },
													{ "lightColor", ValueType::Float4, 16 },
													{ "ambient", ValueType::Float4, 32 } },
													{ { "passColor", ValueType::Float4, 0 } },
													{ { "world", ValueType::Matrix4x4, 0 },
													{ "normalMatrix", ValueType::Matrix4x4, 64 },
													{ "tint", ValueType::Float4, 128 } } };

	// Reserved names and sizes for the common schema buffers which Renderer expects.
	const char* names[]{ "RendererFrame", "RendererView", "RendererLight", "RendererPass", "RendererObject" };
	const UINT size[]{ 16, 224, 48, 16, 144 };

	if (buffer.binding == 5)
	{
		if (buffer.name.starts_with("Renderer"))
		{
			return { false, "CommonSchema: reserved buffer name '" + buffer.name + "'." };
		}

		return {};
	}

	if (buffer.binding > 4 || buffer.name != names[buffer.binding] || buffer.size != size[buffer.binding] ||
		buffer.variables.size() != fields[buffer.binding].size())
	{
		return { false, "CommonSchema: buffer '" + buffer.name + "' does not match the expected schema." };
	}

	for (std::size_t i = 0; i < buffer.variables.size(); ++i)
	{
		const auto& var = buffer.variables[i];
		const auto& field = fields[buffer.binding][i];

		if (var.name != field.name || var.type != field.type || var.offset != field.offset)
		{
			return { false, "CommonSchema: buffer '" + buffer.name + "' variable '" + var.name + "' does not match the expected schema." };
		}

		if (var.type == ValueType::Matrix4x4 && !var.rowMajor)
		{
			return { false, "CommonSchema: buffer '" + buffer.name + "' variable '" + var.name + "' must be row-major." };
		}
	}

	return {};
}

// Check if two shader constant buffers have the same layout (name, binding, size, and variables).

// Used when merging shader reflection data from multiple shaders to ensure consistent buffer layouts.
bool SameLayout(const ShaderConstantBuffer& a, const ShaderConstantBuffer& b) noexcept
{
	if (a.name != b.name || a.binding != b.binding || a.size != b.size
		|| a.variables.size() != b.variables.size())
	{
		return false;
	}

	for (std::size_t i = 0; i < a.variables.size(); i++)
	{
		const auto& varA = a.variables[i];
		const auto& varB = b.variables[i];

		if (varA.name != varB.name || varA.type != varB.type ||
			varA.offset != varB.offset || varA.rowMajor != varB.rowMajor)
		{
			return false;
		}
	}

	return true;
}

}

Result ReflectShader(const ShaderDesc& desc, ShaderContent& output)
{
	if (desc.bytecode.empty())
	{
		return { false, "ReflectShader: bytecode is empty." };
	}

	if (desc.stage != ShaderStage::Vertex && desc.stage != ShaderStage::Pixel)
	{
		return { false, "ReflectShader: unsupported shader stage." };
	}

	// Generate a shader reflection object from the compiled bytecode
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
	HRESULT hr = D3DReflect(desc.bytecode.data(), desc.bytecode.size(), IID_PPV_ARGS(&reflection));

	if (FAILED(hr))
	{
		return { false, "ReflectShader: D3DReflect failed." };
	}

	// Get the reflected shader description
	D3D12_SHADER_DESC shader{};
	if (FAILED(reflection->GetDesc(&shader)))
	{
		return { false, "ReflectShader: GetDesc failed." };
	}

	const auto expectedStage =
		desc.stage == ShaderStage::Vertex ?
		D3D12_SHVER_VERTEX_SHADER : D3D12_SHVER_PIXEL_SHADER;

	if (D3D12_SHVER_GET_TYPE(shader.Version) != expectedStage)
	{
		return { false, "ReflectShader: shader stage mismatch." };
	}

	if (D3D12_SHVER_GET_MAJOR(shader.Version) != 5 || D3D12_SHVER_GET_MINOR(shader.Version) != 0)
	{
		return { false, "ReflectShader: shader version is not 5.0." };
	}

	ShaderContent candidate;	// Temporal storage. Copied to output only if successful.
	candidate.metadata.stage = desc.stage;

	const auto stages =
		desc.stage == ShaderStage::Vertex ?
		ShaderStages::Vertex : ShaderStages::Pixel;

	std::set<std::pair<D3D_SHADER_INPUT_TYPE, UINT>> resourceBindings;

	// Iterate all bound resources which include constant buffers, textures, and samplers.
	// Check if the input is flloat type and parameter is within the allowed range.
	for (UINT i = 0; i < shader.BoundResources; ++i)
	{
		// Get the resource binding description for the current index
		D3D12_SHADER_INPUT_BIND_DESC bindDesc{};

		if (FAILED(reflection->GetResourceBindingDesc(i, &bindDesc)))
		{
			return { false, "ReflectShader: GetResourceBindingDesc failed." };
		}

		if (bindDesc.BindCount != 1 || !bindDesc.Name)
		{
			return { false, "ReflectShader: unsupported resource binding." };
		}

		if (!resourceBindings.emplace(bindDesc.Type, bindDesc.BindPoint).second)
		{
			return { false, "ReflectShader: duplicate resource binding." };
		}

		//----------------------------
		// Constant buffer reflection
		//----------------------------
		if (bindDesc.Type == D3D_SIT_CBUFFER)
		{
			// Get datail by its name.
			auto* nativeBuffer = reflection->GetConstantBufferByName(bindDesc.Name);

			// Get the constant buffer description.
			D3D12_SHADER_BUFFER_DESC bufferDesc{};

			if (!nativeBuffer || FAILED(nativeBuffer->GetDesc(&bufferDesc)))
			{
				return { false, "ReflectShader: GetConstantBufferByName/GetDesc failed." };
			}

			if (bufferDesc.Type != D3D_CT_CBUFFER || !bufferDesc.Size || bufferDesc.Size > 65536)
			{
				return { false, "ReflectShader: unsupported constant buffer." };
			}

			// Convert reflected data to Renderer expression.
			ShaderConstantBuffer cBuffer;
			cBuffer.name = bindDesc.Name;
			cBuffer.binding = bindDesc.BindPoint;
			cBuffer.size = bufferDesc.Size;

			std::set<std::string> variableNames;

			// Iterate all variables in the cbuffer definition.
			for (UINT j = 0; j < bufferDesc.Variables; j++)
			{
				// Get a variable by its index.
				auto* variable = nativeBuffer->GetVariableByIndex(j);

				D3D12_SHADER_VARIABLE_DESC desc{};
				D3D12_SHADER_TYPE_DESC type{};

				// Get the variable description and type description.
				if (!variable || FAILED(variable->GetDesc(&desc)))
				{
					return { false, "ReflectShader: GetVariableByIndex/GetDesc failed." };
				}

				if (!variable->GetType() || FAILED(variable->GetType()->GetDesc(&type)))
				{
					return { false, "ReflectShader: GetType/GetDesc failed." };
				}

				// Convert reflected data and type to Renderer expression.
				ShaderVariable field;
				field.name = desc.Name;
				field.offset = desc.StartOffset;
				field.size = desc.Size;
				field.used = desc.uFlags & D3D_SVF_USED ? true : false;

				// Convert the reflected variable type to Renderer expression.
				auto checked = VariableType(type, field);

				if (!checked.IsSuccess())
				{
					checked.message = "ReflectShader: variable '" + field.name + "': " + checked.message;
					return checked;
				}

				// Check duplication by variable name in the same constant buffer.
				if (!variableNames.insert(field.name).second)
				{
					return { false, "ReflectShader: duplicate variable name '" + field.name + "'." };
				}

				// Check if the variable exceeds the bounds of the constant buffer.
				if (field.offset > cBuffer.size || field.size > cBuffer.size - field.offset)
				{
					return { false, "ReflectShader: variable '" + field.name + "' exceeds buffer bounds." };
				}

				cBuffer.variables.push_back(std::move(field));
			}

			// Check if the constant buffer matches the expected common schema for renderer buffers.
			auto valid = CommonSchema(cBuffer);

			if (!valid.IsSuccess())
			{
				valid.message = "ReflectShader: buffer '" + cBuffer.name + "': " + valid.message;
				return valid;
			}

			// Add the constant buffer to the shader metadata as validated and reflected.
			candidate.metadata.constants.push_back(std::move(cBuffer));
		}
		//----------------------------
		// Texture reflection
		//----------------------------
		else if (bindDesc.Type == D3D_SIT_TEXTURE &&
				 bindDesc.Dimension == D3D_SRV_DIMENSION_TEXTURE2D &&
				 bindDesc.ReturnType == D3D_RETURN_TYPE_FLOAT)
		{
			ShaderResourceBinding resource;
            resource.name = bindDesc.Name;
            resource.binding = bindDesc.BindPoint;
            const bool shadowName = resource.name == "RendererShadowMap";
            const bool shadowSlot = resource.binding == 15;
            if (shadowName != shadowSlot)
            {
                return Fail("RendererShadowMap requires exclusive t15 ownership.");
            }

            if (resource.name.starts_with("Renderer") && !shadowName)
            {
                return Fail("Unknown reserved Renderer texture name.");
            }

            resource.sampler = false;
			resource.stages = stages;
			candidate.metadata.resources.push_back(std::move(resource));
		}
		//----------------------------
		// Sampler reflection
		//----------------------------
		else if (bindDesc.Type == D3D_SIT_SAMPLER &&
				 ((bindDesc.BindPoint <= 1 && !(bindDesc.uFlags & D3D_SIF_COMPARISON_SAMPLER)) ||
                  (bindDesc.BindPoint == 2 && std::string(bindDesc.Name) == "RendererShadowSampler" &&
                   (bindDesc.uFlags & D3D_SIF_COMPARISON_SAMPLER))))
		{
			ShaderResourceBinding resource;
			resource.name = bindDesc.Name;
			resource.binding = bindDesc.BindPoint;
			resource.sampler = true;
			resource.stages = stages;
			candidate.metadata.resources.push_back(std::move(resource));
		}
		//----------------------------
		// Unsupported resource type
		//----------------------------
		else
		{
			return { false, "ReflectShader: unsupported resource or sampler: " + std::string(bindDesc.Name) };
		}
	}
	
	// Iterate all input parameters for vertex shader to reflect the vertex input layout.
	for (UINT i = 0; i < shader.InputParameters && desc.stage == ShaderStage::Vertex; ++i)
	{
		D3D12_SIGNATURE_PARAMETER_DESC inputDesc{};

		// Get the input parameter description for the current index
		if (FAILED(reflection->GetInputParameterDesc(i, &inputDesc)))
		{
			return { false, "ReflectShader: Shader input lookup failed" };
		}

		// The input assembler generates this uint value; no user VB element supplies it.
		// Validate its reflected scalar type, record its presence, then bypass the ordinary semantic layout path.
		if (inputDesc.SystemValueType == D3D_NAME_VERTEX_ID)
		{
			if (inputDesc.ComponentType != D3D_REGISTER_COMPONENT_UINT32 || inputDesc.Mask != 1)
			{
				return Fail("ReflectShader: SV_VertexID must be a scalar uint.");
			}

			candidate.metadata.usesVertexId = true;
			continue;
		}

		if (inputDesc.SystemValueType != D3D_NAME_UNDEFINED)
		{
			return Fail("ReflectShader: unsupported vertex system input.");
		}

		if (inputDesc.ComponentType != D3D_REGISTER_COMPONENT_FLOAT32)
		{
			return { false, "ReflectShader: unsupported vertex component type" };
		}

		UINT count = 0;

		for (UINT bits = inputDesc.Mask; bits; bits >>= 1)
		{
			count += bits & 1;
		}

		if (inputDesc.Mask != ((1u << count) - 1))
		{
			return { false, "ReflectShader: unsupported vertex input mask" };
		}

		// Convert the reflected input parameter to Renderer expression.
		ShaderVertexInput input;
		input.semantic = inputDesc.SemanticName;
		input.index = inputDesc.SemanticIndex;
		input.components = count;

		// Add the vertex input to the shader metadata as validated and reflected.
		candidate.metadata.vertexInputs.push_back(input);
	}

	// Iterate all output parameters for pixel shader to reflect the color output layout.
	// Check the output parameters for pixel shader is SV_Target0 and is a single float4 color output.
	if (desc.stage == ShaderStage::Pixel)
	{
		for (UINT i = 0; i < shader.OutputParameters; ++i)
		{
			D3D12_SIGNATURE_PARAMETER_DESC targetDesc{};

			// Get the output parameter description for the current index
			if (FAILED(reflection->GetOutputParameterDesc(i, &targetDesc)))
			{
				return { false, "ReflectShader: Shader output lookup failed" };
			}

			if (targetDesc.SystemValueType != D3D_NAME_TARGET ||
				targetDesc.SemanticIndex != 0 ||
				targetDesc.ComponentType != D3D10_REGISTER_COMPONENT_FLOAT32)
			{
				return { false, "ReflectShader: R4 supports a single floating color output" };
			}

			// Count the number of components in the output mask (e.g., float4 has 4 components).
			++candidate.metadata.colorOutputs;
		}
	}

	// Copy the bytecode (executable shader code) to the output structure.
	candidate.bytecode.assign(desc.bytecode.begin(), desc.bytecode.end());
	output = std::move(candidate);

	return{};
}

Result MergeMetadata(const ShaderMetadata& vertex, const ShaderMetadata* pixel, ShaderMetadata& output)
{
	ShaderMetadata candidate = vertex;

	if (pixel)
	{
		// Merge constant buffer
		for (const auto& cb: pixel->constants)
		{
			// Collect duplicated bindings for being merged.
			auto found = std::find_if(
				candidate.constants.begin(), candidate.constants.end(),
				[&](const auto& x)
				{
					return x.binding == cb.binding;
				}
			);

			if (found == candidate.constants.end())
			{
				// New constants is added.
				candidate.constants.push_back(cb);
			}
			else
			{
				// Same two constant buffers must have the same layout (name, size, variables)
				// for sharing the b5 safely between vertex and pixel shaders.
				if (!SameLayout(*found, cb))
				{
					return { false, "MergeMetadata: VS/PS cbuffer layout mismatch: " + cb.name };
				}

				for (std::size_t i = 0; i < cb.variables.size(); ++i)
				{
					// Check as used even if one of the shaders uses the variable.
					found->variables[i].used = found->variables[i].used || cb.variables[i].used;
				}
			}
		}

		// Merge bindings (textures and samplers)
		for (const auto& pixelBind: pixel->resources)
		{
			for (const auto& vertexBind : candidate.resources)
			{
				if (vertexBind.sampler == pixelBind.sampler)
				{
					// Same binding requires same name, and same name requires same binding.
					bool sameBinding = vertexBind.binding == pixelBind.binding;
					bool sameName = vertexBind.name == pixelBind.name;

					if (sameBinding != sameName)
					{
						return { false, "MergeMetadata: VS/PS resource name/register collision: " + vertexBind.name + " & " + pixelBind.name };
					}
				}
			}

			// Find same binding reaources
			auto found = std::find_if(
				candidate.resources.begin(), candidate.resources.end(),
				[&](const auto& vertexBind)
				{
					const bool sameResourceKind = vertexBind.sampler == pixelBind.sampler;
					const bool sameRegister = vertexBind.binding == pixelBind.binding;
					return sameResourceKind && sameRegister;
				}
			);

			if (found == candidate.resources.end())
			{
				// New binding is added.
				candidate.resources.push_back(pixelBind);
			}
			else
			{
				// Save which shader stages use this resource (vertex, pixel, or both).
				found->stages = found->stages | pixelBind.stages;
			}
		}

		// Merge color outputs
		candidate.colorOutputs = pixel->colorOutputs;
	}

	// Detect duplicate variable names in the common schema (binding 5) and resource names.
	std::set<std::string> names;

	for (const auto& cb : candidate.constants)
	{
		if (cb.binding == 5)
		{
			for (const auto& var : cb.variables)
			{
				if (!names.insert(var.name).second)
				{
					return { false, "MergeMetadata: duplicate variable name '" + var.name + "' in common schema." };
				}
			}
		}
	}

	// Detect duplicate resource names in the common schema (binding 5) and resource names.
	for (const auto& bind: candidate.resources)
	{
		if (!bind.sampler && ((bind.name.starts_with("Renderer") && bind.name != "RendererShadowMap") || !names.insert(bind.name).second))
		{
			return { false, "MergeMetadata: duplicate resource name '" + bind.name + "' in common schema." };
		}
	}

	output = std::move(candidate);
	return {};
}

Result ValidateMaterial(const MaterialDesc& desc)
{
	std::set<std::string> names;

	// Parameter validation.
	for (const auto& param: desc.parameters)
	{
		// Name validation
		if (param.name.empty())
		{
			return { false, "ValidateMaterial: empty Material parameter" };
		}

		if (param.name.starts_with("Renderer"))
		{
			return { false, "ValidateMaterial: reserved Material parameter" };
		}

		// Check for duplicate parameter names in the material description.
		if (!names.insert(param.name).second)
		{
			return { false, "ValidateMaterial: duplicate Material parameter" };
		}

		// Value type validation
		bool finite = true;

		// Avoiding compile errors for std::visit with non-finite types
		// by using a lambda that checks the type at runtime.
		std::visit(
			[&](const auto& value)
			{
				using T = std::decay_t<decltype(value)>;

				if constexpr (std::is_same_v<T, float>)
				{
					finite = std::isfinite(value);
				}
				else if constexpr (std::is_same_v<T, Vector2>)
				{
					finite = std::isfinite(value.x) && std::isfinite(value.y);
				}
				else if constexpr (std::is_same_v<T, Vector3>)
				{
					finite = std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
				}
				else if constexpr (std::is_same_v<T, Vector4>)
				{
					finite = std::isfinite(value.x) && std::isfinite(value.y) &&
							 std::isfinite(value.z) && std::isfinite(value.w);
				}
				else if constexpr (std::is_same_v<T, Matrix4x4>)
				{
					for (const auto& row : value.m)
					{
						for (float f : row)
						{
							finite = finite && std::isfinite(f);
						}
					}
				}
			},
			param.value
		);

		if (!finite)
		{
			return { false, "ValidateMaterial: non-finite Material value: " + param.name };
		}
	}

	return {};
}

Result PackMaterial(const ShaderConstantBuffer& layout, const MaterialDesc& material, std::vector<std::byte>& output)
{
	// Reserve enough space for the packed material data based on the layout size.
	std::vector<std::byte> candidate(layout.size);

	for (const auto& var: layout.variables)
	{
		// Skip unused fields in the constant buffer layout.
		if (!var.used)
		{
			continue;
		}

		// Check if the material parameter exists for the given variable name in the layout.
		auto found = std::find_if(
			material.parameters.begin(), material.parameters.end(),
			[&](const auto& param)
			{
				return param.name == var.name;
			}
		);

		if (found == material.parameters.end())
		{
			return { false, "PackMaterial: missing Material parameter: " + var.name };
		}

		// Check if the type of the material parameter matches the expected type in the layout.
		if (TypeOf(found->value) != var.type)
		{
			return { false, "PackMaterial: Material type mismatch: " + var.name +
							" expected=" + std::to_string(static_cast<int>(var.type)) +
							" actual=" + std::to_string(static_cast<int>(TypeOf(found->value))) };
		}

		Result status;

		// Treat the Texture, bool, and Matrix4x4 types specially.
		std::visit(
			[&](const auto& value)
			{
				using T = std::decay_t<decltype(value)>;

				if constexpr (std::is_same_v<T, TextureHandle>)
				{
					// TextureHandle is CPU-side only and cannot be packed into the constant buffer.
					status = { false, "PackMaterial: Texture cannot be packed as numeric constant" };
				}
				else
				{
					// Lambda to copy the bytes of the value into the candidate buffer at the correct offset.
					auto store = [&](const void* p, std::size_t count)
						{
							if (var.offset > candidate.size() || count > candidate.size() - var.offset || count > var.size)
							{
								status = { false, "PackMaterial: Material packing range invalid" };
							}
							else
							{
								std::memcpy(candidate.data() + var.offset, p, count);
							}
						};

					if constexpr (std::is_same_v<T, bool>)
					{
						// Convert bool to uint32_t for packing.
						std::uint32_t word = value ? 1u : 0u;
						store(&word, 4);
					}
					else if constexpr (std::is_same_v<T, Matrix4x4>)
					{
						// Convert Matrix4x4 to the correct major order 
						// which reflection expects (row-major or column-major) for packing.
						const Matrix4x4 stored = var.rowMajor ? value : value.Transpose();
						store(&stored, 64);
					}
					else
					{
						// Pack other types directly copying their bytes into the buffer.
						store(&value, sizeof(value));
					}
				}
			},
			found->value);

		if (!status.IsSuccess())
		{
			return status;
		}
	}

	output = std::move(candidate);
	return {};
}

}