#include "AssetManager.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void AssetManager::Initialize(const std::string& projectDir, TextureManager* pTextureManager, MeshManager* pMeshManager)
{
	m_assetRoot = projectDir;
	m_pTextureManager = pTextureManager;
	m_pMeshManager = pMeshManager;
	ScanAssetDirectory(projectDir);
}

void AssetManager::ScanAssetDirectory(const std::string& rootDir)
{
	m_catalog.clear();
	m_pathToId.clear();

	// Check if the root directory exists
	if (!fs::exists(rootDir))
	{
		DBG("AssetManager: Asset root '%s' does not exist.", rootDir.c_str());
		return;
	}

	for (const auto& entry : fs::recursive_directory_iterator(rootDir))
	{
		if (!entry.is_regular_file()) continue; // Skip non-regular files

		const auto& path = entry.path();
		if (path.extension() == ".meta") continue; // Skip .meta files

		// Determine the asset type based on the file extension
		AssetType type = DetermineAssetType(path.extension().string());
		if (type == AssetType::Unknown) continue;	// Skip unknown asset types

		// Resolve or create a GUID for the asset file
		Guid id = ResolveGuidForPath(path.string());

		// Get the relative path of the asset file with respect to the root directory
		std::string relativePath = fs::relative(path, rootDir).generic_string();

		// Build the asset entry
		AssetEntry entry;
		entry.guid = id;
		entry.relativePath = relativePath;
		entry.type = type;

		// Store the asset entry in the catalog and path-to-GUID mapping
		m_catalog[id] = entry;
		m_pathToId[relativePath] = id;
	}

	DBG("AssetManager: Scanned '%s', found %zu assets.", rootDir.c_str(), m_catalog.size());
}

Guid AssetManager::ResolveGuidForPath(const std::string& filePath)
{
	// Return the existing GUID if the .meta file exists
	if (auto existing = MetaFile::TryLoad(filePath))
	{
		return *existing; 
	}

	// Generate a new GUID and save it to the .meta file
	Guid newId = GuidGenerator::Generate();
	MetaFile::Save(filePath, newId);
	return newId;
}

AssetType AssetManager::DetermineAssetType(const std::string& extension)
{
	std::string ext = extension;
	for (auto& c : ext) c = (char)tolower((unsigned char)c);

	// Mesh extensioins
	if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb")
		return AssetType::Mesh;

	// Texture extensions
	if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga")
		return AssetType::Texture;

	return AssetType::Unknown;
}

const AssetEntry* AssetManager::GetAssetEntryByPath(const std::string& relativePath) const
{
	auto it = m_pathToId.find(relativePath);
	if (it == m_pathToId.end()) return nullptr;
	return GetAssetEntry(it->second);
}

const AssetEntry* AssetManager::GetAssetEntry(const Guid& guid) const
{
	auto it = m_catalog.find(guid);
	if (it == m_catalog.end()) return nullptr;
	return &it->second;
}

std::vector<AssetEntry> AssetManager::GetAssetEntries(AssetType type) const
{
	std::vector<AssetEntry> entries;
	
	entries.reserve(m_catalog.size()); // Reserve space to avoid multiple allocations

	// Collect entries of the specified type
	for (const auto& [guid, entry] : m_catalog)
	{
		if (entry.type == type)
		{
			entries.push_back(entry);
		}
	}

	// Sort the entries by their relative paths for consistent ordering
	std::sort(
		entries.begin(), entries.end(), 
		[](const AssetEntry& a, const AssetEntry& b) {
			return a.relativePath < b.relativePath;
		}
	);

	return entries;
}	

MeshHandle AssetManager::GetMeshHandle(const Guid& guid)
{
	auto cashed = m_loadedMeshes.find(guid);
	if (cashed != m_loadedMeshes.end()) return cashed->second; // Return cached handle if already loaded

	// Lookup the asset entry for the given GUID
	const AssetEntry* entryPtr = GetAssetEntry(guid);

	// Return error mesh handle if the asset is unknown or not a mesh
	if (!entryPtr || entryPtr->type != AssetType::Mesh)
	{
		DBG("AssetManager: GetMeshHandle - unknown or non-mesh asset, using error mesh.");
		return m_pMeshManager->GetErrorMeshHandle();
	}

	// Load mesh
	const std::wstring fullPath =
		(fs::path(m_assetRoot) / fs::path(entryPtr->relativePath)).wstring();
	auto& handles = m_pMeshManager->LoadModel(fullPath);

	// If no handles were returned, use the error mesh handle; otherwise, use the first handle
	MeshHandle handle = handles.empty()
		? m_pMeshManager->GetErrorMeshHandle() 
		: handles[0];

	// Cache the loaded mesh handle
	m_loadedMeshes[guid] = handle;
	return handle;
}

TextureHandle AssetManager::GetTextureHandle(const Guid& guid)
{
	// Return cached handle if the texture has already been loaded
	auto cashed = m_loadedTextures.find(guid);
	if (cashed != m_loadedTextures.end()) return cashed->second;

	const AssetEntry* entryPtr = GetAssetEntry(guid);

	if (!entryPtr || entryPtr->type != AssetType::Texture)
	{
		DBG("AssetManager: GetTextureHandle - unknown or non-texture asset, using error texture.");
		return m_pTextureManager->GetErrorTextureHandle();
	}

	// If the texture has not been loaded yet, load it and cache the handle and return it
	const std::wstring fullPath =
		(fs::path(m_assetRoot) / fs::path(entryPtr->relativePath)).wstring();
	TextureHandle handle = m_pTextureManager->LoadTexture(fullPath);
	m_loadedTextures[guid] = handle;
	return handle;
}
