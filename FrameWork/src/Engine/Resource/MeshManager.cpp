#include "MeshManager.h"
#include "Engine/Resource/AssimpLoader.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Core/Debug/Debug.h"

void MeshManager::Initialize(ID3D12Device* pDevice)
{
	m_pDevice = pDevice;
	CreateErrorMesh();
}

const std::vector<MeshHandle>& MeshManager::LoadModel(const std::wstring& path)
{
	// Check if the model is already loaded
	auto it = m_loadedModels.find(path);
	if (it != m_loadedModels.end()) return it->second;	// Return existing handles if already loaded

	// Load meshes
	std::vector<Mesh> meshes;
	ImportSettings settings{ path.c_str(), meshes, false };
	AssimpLoader::Load(settings);

	std::vector<MeshHandle> handles;
	handles.reserve(meshes.size());

	// Process each mesh and create a handle for it
	for (auto& mesh : meshes)
	{
		// Create a mesh handle and store it with the source path
		MeshHandle handle = CreateMeshHandle(mesh);
		m_sorurcePathes[handle] = path;

		// Build material info for the mesh amd store it in the materials map
		MeshMaterialInfo materialInfo;
		materialInfo.materialColor = mesh.materialColor;
		if (!mesh.texPath.empty())
		{
			materialInfo.textureHandle = TextureManager::GetInstance()->LoadTexture(mesh.texPath); 
		}
		m_materials[handle] = materialInfo;

		// Store the handle in the handles vector
		handles.push_back(handle);
	}
}

MeshHandle MeshManager::CreateMeshHandle(Mesh& src)
{
	MeshHandle handle = m_nextMeshHandle++;
	m_meshes[handle] = std::make_unique<MeshGPU>(m_pDevice, src);
	return handle;
}

MeshGPU* MeshManager::GetMeshGPU(MeshHandle handle)
{
	auto it = m_meshes.find(handle);
	if (it != m_meshes.end()) return it->second.get();
	return nullptr;
}

MeshMaterialInfo MeshManager::GetMeshMaterialInfo(MeshHandle handle)
{
	auto it = m_materials.find(handle);
	if (it != m_materials.end()) return it->second;
	return MeshMaterialInfo{};
}

std::wstring MeshManager::GetSourcePath(MeshHandle handle)
{
	auto it = m_sorurcePathes.find(handle);
	if (it != m_sorurcePathes.end()) return it->second;
	return L"";
}

void MeshManager::CreateErrorMesh()
{
	// Create cube mesh for error mesh
	Model cubeModel = RenderTemplateFactory::LoadDefaultModel(DEFAULT_MESH::CUBE);
	if (!cubeModel.empty())
	{
		DBG("MeshManager: Failed to create error mesh (default cube model is empty).");
		return;
	}

	m_errorMeshHandle = CreateMeshHandle(cubeModel[0]);	// Store mesh and handle

	// Load texture for error mesh
	TextureHandle errorTex = TextureManager::GetInstance()->LoadTexture(L"asset/texture/error_checker.png");
	
	// Store material info for error mesh
	MeshMaterialInfo materialInfo;
	materialInfo.textureHandle = errorTex = errorTex;
	materialInfo.materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_materials[m_errorMeshHandle] = materialInfo;

	DBG("MeshManager: Error mesh created (handle=%u).", m_errorMeshHandle);
}