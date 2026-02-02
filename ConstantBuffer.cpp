#include "ConstantBuffer.h"
#include "Engine.h"

ConstantBuffer::ConstantBuffer(ID3D12Device* pDevice, size_t size)
{
	size_t align = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;	//256バイトアライメント
	UINT64 sizeAligned = (size + (align - 1)) & ~(align - 1);		//alignに切り上げる.

	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);	//ヒーププロパティ
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeAligned);		//リソースの設定

	//定数バッファの生成
	result = pDevice->CreateCommittedResource(
		&heapProps,									//ヒーププロパティ
		D3D12_HEAP_FLAG_NONE,						//ヒープフラグ
		&bufferDesc,								//リソースの設定
		D3D12_RESOURCE_STATE_GENERIC_READ,			//リソースの使用状態
		nullptr,									//最適化されたクリア値(不要なのでnullptr)
		IID_PPV_ARGS(&m_pBuffer)					//バッファのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	//生成に失敗した場合は終了
	if (FAILED(result))
	{
		return;
	}

	//マッピング
	result = m_pBuffer->Map(0, nullptr, &m_pMappedPtr);
	if (FAILED(result))
	{
		return;
	}

	//定数バッファビューの設定
	m_Desc.BufferLocation = m_pBuffer->GetGPUVirtualAddress();	//バッファのGPU仮想アドレスを取得
	m_Desc.SizeInBytes = static_cast<UINT>(sizeAligned);		//バッファのサイズを設定

	//バッファの生成に成功
	m_IsValid = true;
}

//バッファ生成に成功したかを返す
bool ConstantBuffer::GetIsValid()
{
	return m_IsValid;
}

//バッファのGPU上のアドレスを返す
D3D12_GPU_VIRTUAL_ADDRESS ConstantBuffer::GetAddress() const
{
	return m_Desc.BufferLocation;
}

//定数バッファビューを返す
D3D12_CONSTANT_BUFFER_VIEW_DESC ConstantBuffer::GetViewDesc()
{
	return m_Desc;
}

//定数バッファにマッピングされたポインタを返す
void* ConstantBuffer::GetPtr() const
{
	return m_pMappedPtr;
}
