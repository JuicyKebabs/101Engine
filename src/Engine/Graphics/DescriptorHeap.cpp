#include "DescriptorHeap.h"

void DescriptorHeap::Initialize(ID3D12Device* pDevice, DescriptorHeap::Type type, UINT numDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = ConvertToD3D12Type(type);
	desc.NumDescriptors = numDescriptors;
	desc.Flags = (type == Type::CBV_SRV_UAV || type == Type::SAMPLER) ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
	if (FAILED(hr))
	{
		assert(false && "Failed to create descriptor heap");
	}

	m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(desc.Type);
	m_numDescriptors = numDescriptors;
	m_nextFreeIndex = 0;
	m_type = type;
}