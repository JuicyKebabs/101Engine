#pragma once
#include <d3d12.h>
#include "Engine/Graphics/VertexBuffer.h"
#include "Engine/Graphics/IndexBuffer.h"
#include "Engine/Core/Utility/SharedStruct.h"
#include "Engine/Graphics/RenderData.h"

//メッシュ情報をGPU上に保持するクラス
class MeshGPU
{
public:
	MeshGPU(	//コンストラクタ
		ID3D12Device* pDevice,	//デバイス
		Mesh& src		//メッシュデータ
	);
	~MeshGPU();	//デストラクタ

	//ゲッター
	VertexBuffer* GetVertexBuffer() const { return m_pVertexBuffer; }	//頂点バッファを返す
	IndexBuffer* GetIndexBuffer() const { return m_pIndexBuffer; }		//インデックスバッファを返す
	UINT GetIndexCount() const { return m_IndexCount; }					//インデックス数を返す
	D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_Topology; }	//プリミティブトポロジを返す
	float GetSortRadius() const { return m_sortRadius; }				//ソート用の半径を返す

private:
	VertexBuffer* m_pVertexBuffer = nullptr;	//頂点バッファ
	IndexBuffer* m_pIndexBuffer = nullptr;		//インデックスバッファ
	UINT m_IndexCount = 0;						//インデックス数
	D3D12_PRIMITIVE_TOPOLOGY m_Topology = 
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;	//プリミティブトポロジ

	float m_sortRadius = 0.0f;	//ソート用の半径
};
