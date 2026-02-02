#include "VertexBuffer.h"
#include "Engine.h"

// コンストラクタでバッファを生成
VertexBuffer::VertexBuffer(ID3D12Device* pDevice, size_t size, size_t stride, const void* pInitData)
{
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);	//ヒーププロパティ
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);				//バッファの設定

	// 頂点バッファの生成
	result = pDevice->CreateCommittedResource(
		&heapProps,									//ヒーププロパティ
		D3D12_HEAP_FLAG_NONE,						//ヒープフラグ
		&bufferDesc,								//リソースの設定
		D3D12_RESOURCE_STATE_GENERIC_READ,			//リソースの使用状態
		nullptr,									//最適化されたクリア値(不要なのでnullptr)
		IID_PPV_ARGS(&m_Buffer)				//バッファのアドレスを取得(IID_PPV_ARGSマクロでオブジェクトの型を特定)
	);

	//生成に失敗した場合は終了
	if (FAILED(result))
	{
		return;
	}

	//頂点バッファビューの設定
	m_BufferView.BufferLocation = 
		m_Buffer->GetGPUVirtualAddress();						//バッファのGPU仮想アドレスを取得
	m_BufferView.SizeInBytes = static_cast<UINT>(size);		//バッファのサイズを設定
	m_BufferView.StrideInBytes = static_cast<UINT>(stride);	//頂点の1要素分のサイズを設定

	//マッピング
	if (pInitData != nullptr)
	{
		void* pMappedData = nullptr; // マッピング後のポインタ

		//バッファをマッピング
		result = m_Buffer->Map(0, nullptr, &pMappedData);

		//マッピングに成功した場合
		if (SUCCEEDED(result))
		{
			//初期データをコピー
			memcpy(pMappedData, pInitData, size);
			//アンマッピング
			m_Buffer->Unmap(0, nullptr);
			m_IsValid = true; // バッファの生成に成功
		}
	}

	//バッファの生成に成功
	m_IsValid = true;
}

//バッファの生成に成功したかを返す
bool VertexBuffer::GetIsValid() const
{
	return m_IsValid;
}

//頂点バッファビューを返す
const D3D12_VERTEX_BUFFER_VIEW& VertexBuffer::GetView() const
{
	return m_BufferView;
}
