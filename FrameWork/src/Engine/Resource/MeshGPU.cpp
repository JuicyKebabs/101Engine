#include "MeshGPU.h"

//コンストラクタ
MeshGPU::MeshGPU(ID3D12Device* pDevice, Mesh& src)
{
	//頂点バッファの生成
	const size_t vbsize = sizeof(Vertex) * src.vertexCount;	//頂点バッファサイズ
	const size_t vstride = sizeof(Vertex);					//頂点バッファの1頂点あたりのサイズ
	m_pVertexBuffer = new VertexBuffer(	//頂点バッファの生成
		pDevice,				//デバイス
		vbsize,					//頂点バッファサイズ
		vstride,				//頂点バッファの1頂点あたりのサイズ
		src.vertices.data()		//頂点データ
	);

	//インデックスバッファの生成
	const size_t ibsize = sizeof(uint32_t) * src.indexCount;	//インデックスバッファサイズ
	m_pIndexBuffer = new IndexBuffer(	//インデックスバッファの生成
		pDevice,				//デバイス
		ibsize,					//インデックスバッファサイズ
		src.indices.data()		//インデックスデータ
	);

	//インデックス数の設定
	m_IndexCount = static_cast<UINT>(src.indexCount);
	//プリミティブトポロジの設定
	m_Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//ソート用の半径を計算
	float maxDistSq = 0.0f; //原点から頂点までの最大距離の二乗
	for (size_t i = 0; i < src.vertexCount; i++)
	{
		//原点から最も遠い距離を保存
		const auto& p = src.vertices[i].position;
		float len2 = p.x * p.x + p.y * p.y + p.z * p.z;
		maxDistSq = std::max(maxDistSq, len2);
	}
	m_sortRadius = sqrt(maxDistSq);
}

//デストラクタ
MeshGPU::~MeshGPU()
{
	delete m_pVertexBuffer;
	delete m_pIndexBuffer;
}
