#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include "MeshGPU.h"
#include "MeshHandle.h"
#include "Engine/Resource/Texture.h"
#include "Engine/Core/Math/Math.h"

//-----------------------------------------------------------------------------
// MeshManager class
// This class manages meshes and their associated materials with it's handles
// Uploads mesh data to the GPU and keeps track of loaded meshes and materials
//-----------------------------------------------------------------------------


// Structure to hold material information linked to a mesh
struct MeshMaterialInfo
{
	TextureHandle textureHandle = InvalidTextureHandle;	// Texture handle
	Vector4 materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };	// Material color (RGBA)
};

// Mesh management class
class MeshManager
{
public:
	MeshManager() = default;	// Constructor
	~MeshManager() = default;	// Destructor

	// Get singleton instance
	static MeshManager* GetInstance()
	{
		static MeshManager instance;
		return &instance;
	}

	void Initialize(ID3D12Device* pDevice);	// Initialize

	// Load a model from a file and return its mesh handles
	// If the model is already loaded, it returns the existing handles
	const std::vector<MeshHandle>& LoadModel(const std::wstring& path);

	// Create a mesh from a Mesh structure and return its handle
	// (Called in LoadModel function or built-in mesh creation)
	MeshHandle CreateMeshHandle(Mesh& src);

	MeshGPU* GetMeshGPU(MeshHandle handle);						// Get the MeshGPU associated with a mesh handle
	MeshMaterialInfo GetMeshMaterialInfo(MeshHandle handle);	// Get the material info associated with a mesh handle

	// Get the source path of a mesh handle(for debbuging or serialization purposes)
	std::wstring GetSourcePath(MeshHandle handle);

private:

	std::unordered_map<std::wstring, std::vector<MeshHandle>> m_loadedModels;	// Path to submesh handles mapping
	std::unordered_map<MeshHandle, std::unique_ptr<MeshGPU>> m_meshes;			// Mesh handle to MeshGPU mapping
	std::unordered_map<MeshHandle, std::wstring> m_sorurcePathes;				// Mesh handle to source path mapping
	std::unordered_map<MeshHandle, MeshMaterialInfo> m_materials;				// Mesh handle to material info mapping

	MeshHandle m_nextMeshHandle = 0;	// Next available mesh handle

	ID3D12Device* m_pDevice = nullptr;	// Pointer to the device
};