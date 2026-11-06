#include "PipelineGpu.h"
#include "ApiResult.h"
#include "Renderer/Geometry.h"

#include <algorithm>

namespace rendering::backend
{

Result CreateRootSignature(
	ID3D12Device* device,
	const D3D12_ROOT_SIGNATURE_DESC& desc,
	Microsoft::WRL::ComPtr<ID3D12RootSignature>& output)
{
	if (!device)
	{
		return {false, "CreateRootSignature: device is null."};
	}

	if (output)
	{
		return {false, "CreateRootSignature: output already contains a root signature."};
	}

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

	if (FAILED(hr))
	{
		std::string errorMessage = "CreateRootSignature: D3D12SerializeRootSignature failed.";

		if (errorBlob)
		{
			errorMessage += " Error: " + std::string(static_cast<const char*>(errorBlob->GetBufferPointer()),
                                                     errorBlob->GetBufferSize());
		}

		return MakeApiFailure(errorMessage.c_str(), hr);
	}

	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
                                     IID_PPV_ARGS(&output));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateRootSignature: ID3D12Device::CreateRootSignature failed.", hr);
	}

	return {};
}

Result CreateGraphicsPipeline(
	ID3D12Device* device,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
	Microsoft::WRL::ComPtr<ID3D12PipelineState>& output)
{
	if (!device)
	{
		return {false, "CreateGraphicsPipeline: device is null."};
	}

	if (output)
	{
		return {false, "CreateGraphicsPipeline: output already contains a pipeline state."};
	}

	auto hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&output));

	if (FAILED(hr))
	{
		return MakeApiFailure("CreateGraphicsPipeline: ID3D12Device::CreateGraphicsPipelineState failed.", hr);
	}

	return {};
}

std::vector<VertexField> VertexFields()
{
	MeshVertex vertex;

	auto offset = [&](const void* member)
	{ return static_cast<UINT>(static_cast<const char*>(member) - reinterpret_cast<const char*>(&vertex)); };

	std::vector<VertexField> fields;
	fields.push_back(VertexField{"POSITION", 3, offset(&vertex.position), DXGI_FORMAT_R32G32B32_FLOAT});
	fields.push_back(VertexField{"NORMAL", 3, offset(&vertex.normal), DXGI_FORMAT_R32G32B32_FLOAT});
	fields.push_back(VertexField{"TEXCOORD", 2, offset(&vertex.uv), DXGI_FORMAT_R32G32_FLOAT});
	fields.push_back(VertexField{"TANGENT", 3, offset(&vertex.tangent), DXGI_FORMAT_R32G32B32_FLOAT});
	fields.push_back(VertexField{"COLOR", 4, offset(&vertex.color), DXGI_FORMAT_R32G32B32A32_FLOAT});

	return fields;
}

namespace
{

DXGI_FORMAT NativeFormat(graph::GraphTextureFormat format)
{
	switch (format)
	{
	case graph::GraphTextureFormat::RGBA8_UNorm:
		return DXGI_FORMAT_R8G8B8A8_UNORM;

	case graph::GraphTextureFormat::RGBA8_sRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	case graph::GraphTextureFormat::RGBA16_Float:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;

	case graph::GraphTextureFormat::D32_Float:
		return DXGI_FORMAT_D32_FLOAT;

	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

Result BuildRootSignature(
	ID3D12Device* device,
	const PipelineBuildInputs& inputs,
	Microsoft::WRL::ComPtr<ID3D12RootSignature>& output)
{
	std::vector<D3D12_ROOT_PARAMETER> rootParameters;
	std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;

	const auto textureCount = std::count_if(inputs.layout->resources.begin(), inputs.layout->resources.end(),
											[](const auto& x)
											{
												return !x.sampler; // Count only texture resources, not samplers.
											});

	descriptorRanges.reserve(textureCount);
	rootParameters.reserve(inputs.layout->constants.size() + textureCount);

	for (const auto& cb : inputs.layout->constants)
	{
		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		param.Descriptor.ShaderRegister = cb.binding;
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters.push_back(param);
	}

	for (const auto& resource : inputs.layout->resources)
	{
		if (resource.sampler)
		{
			continue;
		}

		D3D12_DESCRIPTOR_RANGE range{};
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		range.NumDescriptors = 1;
		range.BaseShaderRegister = resource.binding;
		range.RegisterSpace = 0;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		descriptorRanges.push_back(range);

		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.DescriptorTable.NumDescriptorRanges = 1;
		param.DescriptorTable.pDescriptorRanges = &descriptorRanges.back();
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParameters.push_back(param);
	}

	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;

	for (const auto& sampler : inputs.layout->resources)
	{
		if (!sampler.sampler)
		{
			continue;
		}

		D3D12_STATIC_SAMPLER_DESC staticSampler{};
		staticSampler.ShaderRegister = sampler.binding;
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

		auto addressMode = sampler.binding == 0 ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSampler.AddressU = addressMode;
		staticSampler.AddressV = addressMode;
		staticSampler.AddressW = addressMode;

		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		if (sampler.binding == 2)
		{
			// Comparison is part of the reserved Shadow sampler contract.
			// Ordinary s0/s1 retain their existing wrap/clamp interpretation.
			staticSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		}

		staticSampler.MaxAnisotropy = 1;
		staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		staticSamplers.push_back(staticSampler);
	}

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.NumParameters = static_cast<UINT>(rootParameters.size());
	rootSigDesc.pParameters = rootParameters.data();
	rootSigDesc.NumStaticSamplers = static_cast<UINT>(staticSamplers.size());
	rootSigDesc.pStaticSamplers = staticSamplers.data();
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	return CreateRootSignature(device, rootSigDesc, output);
}

Result BuildInputLayout(
	std::span<const ShaderVertexInput> vertexInputs,
	std::vector<D3D12_INPUT_ELEMENT_DESC>& inputElements)
{
	auto fields = VertexFields();

	for (const auto& required : vertexInputs)
	{
		auto field = std::find_if(fields.begin(), fields.end(),
                                  [&](const VertexField& f) { return f.semantic == required.semantic; });

		if (field == fields.end())
		{
			return {false, "BuildPipeline: unsupported vertex semantic."};
		}

		D3D12_INPUT_ELEMENT_DESC element{};
		element.SemanticName = field->semantic;
		element.SemanticIndex = 0;
		element.Format = field->format;
		element.InputSlot = 0;
		element.AlignedByteOffset = field->offset;
		element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		element.InstanceDataStepRate = 0;

		inputElements.push_back(element);
	}

	return {};
}

} // namespace

Result BuildPipeline(ID3D12Device* device, const PipelineBuildInputs& inputs, PipelineGpuData& output)
{
	Result prepared = BuildRootSignature(device, inputs, output.rootSignature);

	if (!prepared.IsSuccess())
	{
		return prepared;
	}

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
	prepared = BuildInputLayout(inputs.vertex->metadata.vertexInputs, inputElements);

	if (!prepared.IsSuccess())
	{
		return prepared;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = output.rootSignature.Get();
	psoDesc.VS = {inputs.vertex->bytecode.data(), inputs.vertex->bytecode.size()};

	if (inputs.pixel)
	{
		psoDesc.PS = {inputs.pixel->bytecode.data(), inputs.pixel->bytecode.size()};
	}

	psoDesc.InputLayout = {inputElements.data(), static_cast<UINT>(inputElements.size())};
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.SampleDesc.Count = inputs.output->sampleCount;
	psoDesc.SampleDesc.Quality = inputs.output->sampleQuality;
	psoDesc.RasterizerState.FillMode =
		inputs.description->fill == FillMode::Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;

	auto cullMode = inputs.description->cull == CullMode::None
						? D3D12_CULL_MODE_NONE
						: (inputs.description->cull == CullMode::Back ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT);

	psoDesc.RasterizerState.CullMode = cullMode;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;
	psoDesc.DepthStencilState.DepthEnable = inputs.depth != DepthMode::Disabled;

	auto depthWriteMask =
		inputs.depth == DepthMode::ReadWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;

	psoDesc.DepthStencilState.DepthWriteMask = depthWriteMask;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	psoDesc.DepthStencilState.FrontFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
                                           D3D12_COMPARISON_FUNC_ALWAYS};
	psoDesc.DepthStencilState.BackFace = psoDesc.DepthStencilState.FrontFace;

	auto& blendDesc = psoDesc.BlendState.RenderTarget[0];
	blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	auto srcBlend = inputs.description->blend == BlendMode::Alpha ? D3D12_BLEND_SRC_ALPHA : D3D12_BLEND_ONE;
	auto destBlend = inputs.description->blend == BlendMode::Alpha ? D3D12_BLEND_INV_SRC_ALPHA : D3D12_BLEND_ZERO;

	blendDesc.SrcBlend = srcBlend;
	blendDesc.DestBlend = destBlend;
	blendDesc.BlendEnable = inputs.description->blend == BlendMode::Alpha;
	blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;

	psoDesc.NumRenderTargets = static_cast<UINT>(inputs.output->colorFormats.size());

	for (std::size_t i = 0; i < inputs.output->colorFormats.size(); ++i)
	{
		psoDesc.RTVFormats[i] = NativeFormat(inputs.output->colorFormats[i]);
	}

	psoDesc.DSVFormat = inputs.output->depthFormat ? NativeFormat(*inputs.output->depthFormat) : DXGI_FORMAT_UNKNOWN;

	return CreateGraphicsPipeline(device, psoDesc, output.pipelineState);
}

} // namespace rendering::backend