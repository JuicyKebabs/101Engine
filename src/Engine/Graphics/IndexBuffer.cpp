#include "IndexBuffer.h"
#include "Engine/Engine.h"

//コンストラクタでバッファを生成
IndexBuffer::IndexBuffer(ID3D12Device* pDevice, size_t size, const void* pInitData)
{
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);	//アップロード可能なヒーププロパティを作成
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);			//バッファのリソース設定を作成

	//バッファの生成
	result = pDevice->CreateCommittedResource(
		&heapProps,							//ヒーププロパティ
		D3D12_HEAP_FLAG_NONE,				//ヒープフラグ
		&resourceDesc,						//リソース設定
		D3D12_RESOURCE_STATE_GENERIC_READ,	//リソースの使用方法
		nullptr,							//最適化されたクリア値(不要なのでnullptr)
		IID_PPV_ARGS(&m_buffer)				//生成するリソースポインタの受け取り
	);

	if(FAILED(result))
	{
		m_IsValid = false;
		return;
	}

	//バッファビューの作成
	m_BufferView.BufferLocation = m_buffer->GetGPUVirtualAddress();	//バッファのGPU仮想アドレス
	m_BufferView.SizeInBytes = static_cast<UINT>(size);				//バッファのサイズ
	m_BufferView.Format = DXGI_FORMAT_R32_UINT;						//インデックスフォーマット(32ビット整数)

	//マッピングしてデータ転送
	if(pInitData != nullptr)
	{
		void* pMappedData = nullptr;
		//CD3DX12_RANGE readRange(0, 0); //読み取り範囲(書き込みのみなので0サイズ)
		//マッピング
		result = m_buffer->Map(0, nullptr, &pMappedData);
		if (FAILED(result))
		{
			m_IsValid = false;
			return;
		}

		//データ転送
		memcpy(pMappedData, pInitData, size);
		//アンマッピング
		//CD3DX12_RANGE writeRange(0, size); //書き込み範囲
		m_buffer->Unmap(0, nullptr);
	}

	m_IsValid = true;
}

//インデックスバッファビューを返す
D3D12_INDEX_BUFFER_VIEW IndexBuffer::GetView() const
{
	return m_BufferView;
}
