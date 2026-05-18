#pragma once
#include <vector>
#include "MeshGPU.h"

// Mesh management class
class MeshManager
{
public:
	MeshManager() {};	// Constructor
	~MeshManager();		// Destructor

	// Get singleton instance
	static MeshManager* GetInstance()
	{
		static MeshManager instance;
		return &instance;
	}

	void Initialize(ID3D12Device* pDevice);	// Initialize

	MeshGPU* CreateMesh(Mesh& src);	// Create a mesh, add it to the list, and return a pointer to the mesh

private:
	std::vector<MeshGPU*> m_Meshes;	// List of meshes

	ID3D12Device* m_pDevice = nullptr;	// Pointer to the device
};