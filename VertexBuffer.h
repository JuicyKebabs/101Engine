#pragma once
#include "d3dx12.h"
#include <DirectXMath.h>
#include "ComPtr.h"

class VertexBuffer
{
public:	//公開関数
	VertexBuffer() {};	//デフォルトコンストラクタ
	VertexBuffer(ID3D12Device* pDevice, size_t size, size_t stride, const void* pInitData); // コンストラクタでバッファを生成
	~VertexBuffer() {};	//デストラクタ

	//ゲッター
	bool GetIsValid() const;							//バッファの生成に成功したかを返す
	const D3D12_VERTEX_BUFFER_VIEW& GetView() const;	//頂点バッファビューを返す

	VertexBuffer(const VertexBuffer&) = delete;		// コピーコンストラクタ禁止
	void operator = (const VertexBuffer&) = delete;	// 代入演算子禁止

private:
	bool m_IsValid = false;						// バッファの生成に成功したかを取得
	ComPtr<ID3D12Resource> m_Buffer;			// 頂点バッファ
	D3D12_VERTEX_BUFFER_VIEW m_BufferView{};	// 頂点バッファビュー

private:	//結果コード
	HRESULT result = S_OK;	// HRESULT(成功/失敗コード)


};