#include "RootSignature.h"
#include "Engine/Engine.h"

RootSignature::RootSignature(ID3D12Device* pDevice)
{
	// Setting the root signature flags
	auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
	flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_PARAMETER rootParam[4] = {}; // Root parameters

	// b0 : Frame constants
	rootParam[0].InitAsConstantBufferView(0);

	// b1 : Mesh and Sprite render constants
	rootParam[1].InitAsConstantBufferView(1);

	// b2 : Liting constants
	rootParam[2].InitAsConstantBufferView(2);

	// t0 : Textures
	CD3DX12_DESCRIPTOR_RANGE srvRange;

	// Setting up the descriptor range for shader resource views (SRV)
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1 ,0);
	rootParam[3].InitAsDescriptorTable(1, &srvRange);

	// Setting up a static sampler for linear filtering
	auto sampler = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	// Descriptoion structure for the root signature
	CD3DX12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = _countof(rootParam);	// Number of root parameters
	desc.NumStaticSamplers = 1;					// Number of static samplers
	desc.pParameters = rootParam;				// Pointer to the array of root parameters
	desc.pStaticSamplers = &sampler;			// Pointer to the array of static samplers
	desc.Flags = flag;							// Root signature flags

	// Serialize the root signature
	ComPtr<ID3DBlob> pBlob;		// Blob for the serialized root signature
	ComPtr<ID3DBlob> pError;	// Blob for error messages
	result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pBlob, &pError);

	// Check if serialization succeeded
	if(FAILED(result))
	{
		if (pError)
		{
			OutputDebugStringA((char*)pError->GetBufferPointer());
			m_isValid = false;
		}
		return;
	}

	// Create the root signature
	result = pDevice->CreateRootSignature(
		0,								// Node mask (for multi-GPU, 0 means all nodes)
		pBlob->GetBufferPointer(),		// Pointer to the serialized root signature
		pBlob->GetBufferSize(),			// Size of the serialized root signature
		IID_PPV_ARGS(&m_rootSignature)	// Output pointer for the created root signature (IID_PPV_ARGS macro to specify the type)
	);

	if (FAILED(result))
	{
		OutputDebugStringA("RootSignature : Failed to create root signature.\n");
		m_isValid = false;
		return;
	}

	m_isValid = true; // Root signature creation succeeded
}

ID3D12RootSignature* RootSignature::GetRootSignature() const
{
	return m_rootSignature.Get();
}
