#pragma once
#include <cstdint>
#include <d3d12.h>
#include "Engine/Core/ComPtr/ComPtr.h"

//インデックスクラス
class IndexBuffer
{
private:
	bool m_IsValid = false;					//バッファの生成に成功したかを取得
	ComPtr<ID3D12Resource> m_buffer;		//インデックスバッファ
	D3D12_INDEX_BUFFER_VIEW m_BufferView{};	//インデックスバッファビュー

private:	//結果コード
	HRESULT result = S_OK;	//HRESULT(成功/失敗コード)

public:
	IndexBuffer(ID3D12Device* pDevice, size_t size, const void* pInitData); // コンストラクタでバッファを生成
	~IndexBuffer() {};	//デストラクタ

	//ゲッター
	D3D12_INDEX_BUFFER_VIEW GetView() const;		//インデックスバッファビューを返す
};