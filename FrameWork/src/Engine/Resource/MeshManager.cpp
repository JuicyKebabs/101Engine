#include "MeshManager.h"
#include "Engine/Resource/AssimpLoader.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Graphics/RenderTemplateFactory.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <algorithm>

void MeshManager::Initialize(ID3D12Device* pDevice, TextureManager* pTextureManager)
{
	m_pDevice = pDevice;
	m_pTextureManager = pTextureManager;
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
			materialInfo.textureHandle = m_pTextureManager->LoadTexture(mesh.texPath); 
		}
		m_materials[handle] = materialInfo;

		// Store the handle in the handles vector
		handles.push_back(handle);
	}

	return m_loadedModels.emplace(path, std::move(handles)).first->second;
}

MeshHandle MeshManager::CreateMeshHandle(Mesh& src)
{
	if (src.boundsRadius <= 0.0f && !src.vertices.empty())
	{
		Vector3 minPos = src.vertices.front().position;
		Vector3 maxPos = minPos;
		for (const Vertex& vertex : src.vertices)
		{
			const Vector3& pos = vertex.position;
			minPos.x = std::min(minPos.x, pos.x);
			minPos.y = std::min(minPos.y, pos.y);
			minPos.z = std::min(minPos.z, pos.z);
			maxPos.x = std::max(maxPos.x, pos.x);
			maxPos.y = std::max(maxPos.y, pos.y);
			maxPos.z = std::max(maxPos.z, pos.z);
		}
		src.boundsCenter = (minPos + maxPos) * 0.5f;
		src.boundsRadius = (maxPos - minPos).Length() * 0.5f;
	}

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
	Model cubeModel = RenderTemplateFactory::LoadDefaultModel(DefaultMesh::Cube);
	if (cubeModel.empty())
	{
		DBG("MeshManager: Failed to create error mesh (default cube model is empty).");
		return;
	}

	m_errorMeshHandle = CreateMeshHandle(cubeModel[0]);	// Store mesh and handle

	// Load texture for error mesh
	TextureHandle errorTex = m_pTextureManager->LoadTexture(PathManager::ResolveW("asset/texture/error_checker.png"));

	// Store material info for error mesh
	MeshMaterialInfo materialInfo;
	materialInfo.textureHandle = errorTex;
	materialInfo.materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_materials[m_errorMeshHandle] = materialInfo;

	DBG("MeshManager: Error mesh created (handle=%u).", m_errorMeshHandle);
}

MeshHandle MeshManager::LoadDefaultMesh(DefaultMesh type)
{
	const auto loaded = m_loadedDefaultMeshes.find(type);
	if (loaded != m_loadedDefaultMeshes.end()) return loaded->second;

	Model model = GetDefaultModel(type);
	if (model.empty()) return m_errorMeshHandle;

	MeshHandle handle = CreateMeshHandle(model.front());
	m_materials[handle] = MeshMaterialInfo{};
	m_loadedDefaultMeshes.emplace(type, handle);
	return handle;
}
