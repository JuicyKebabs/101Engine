#include "MeshManager.h"

//デストラクタ
MeshManager::~MeshManager()
{
	for (auto mesh : m_Meshes)
	{
		delete mesh;
	}
	m_Meshes.clear();
}

//初期化
void MeshManager::Initialize(ID3D12Device* pDevice)
{
	m_pDevice = pDevice;
}

//メッシュを作成してリストに追加、メッシュへのポインタを返す
MeshGPU* MeshManager::CreateMesh(Mesh& src)
{
	MeshGPU* newMesh = new MeshGPU(m_pDevice, src);	//メッシュを作成
	m_Meshes.push_back(newMesh);					//リストに追加
	return newMesh;									//メッシュへのポインタを返す
}