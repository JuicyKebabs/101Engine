#pragma once
#include <d3d12.h>
#include "Engine/Graphics/VertexBuffer.h"
#include "Engine/Graphics/IndexBuffer.h"
#include "Engine/Graphics/RenderData.h"

//----------------------------------------------------------
// MeshGPU class
// This class represents a mesh stored in GPU memory
// Saving all datas about mesh which are used for rendering
//----------------------------------------------------------

class MeshGPU
{
public:
	MeshGPU(ID3D12Device* pDevice, Mesh& src);
	~MeshGPU();

	// Getters
	VertexBuffer* GetVertexBuffer() const { return m_pVertexBuffer; }
	IndexBuffer* GetIndexBuffer() const { return m_pIndexBuffer; }
	UINT GetIndexCount() const { return m_IndexCount; }
	D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_Topology; }
	float GetSortRadius() const { return m_sortRadius; }

private:
	VertexBuffer* m_pVertexBuffer = nullptr;	// Vertex buffer
	IndexBuffer* m_pIndexBuffer = nullptr;		// Index buffer
	UINT m_IndexCount = 0;						// Index count
	D3D12_PRIMITIVE_TOPOLOGY m_Topology = 
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;	// Primitive topology

	float m_sortRadius = 0.0f;	// Sort radius
};
