#include "AssetManager.h"
#include "Engine/Core/GUID/GuidGenerator.h"
#include "Engine/Resource/MetaFile.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace
{
	bool FailCatalog(AssetCatalogError* error, AssetCatalogErrorCode code, std::string path, std::string message)
	{
		DBG("AssetManager: %s: %s", path.c_str(), message.c_str());
		if (error) *error = { code, std::move(path), std::move(message) };
		return false;
	}
}

bool AssetManager::Initialize(const std::string& projectDir, TextureManager* pTextureManager,
	MeshManager* pMeshManager, AssetCatalogError* outError)
{
	if (!ScanAssetDirectory(projectDir, "", outError)) return false;
	m_pTextureManager = pTextureManager;
	m_pMeshManager = pMeshManager;
	m_loadedMeshes.clear();
	m_loadedTextures.clear();
	return true;
}

bool AssetManager::Refresh(AssetCatalogError* outError)
{
	return ScanAssetDirectory(m_assetRoot, "", outError);
}

bool AssetManager::NotifyAssetChanged(const std::string& relativePath, AssetCatalogError* outError)
{
	const fs::path path(relativePath);
	if (path.empty() || path.is_absolute() || path.has_root_name() ||
		std::find(path.begin(), path.end(), fs::path("..")) != path.end() || path.lexically_normal() == ".")
	{
		return FailCatalog(outError, AssetCatalogErrorCode::InvalidPath, relativePath, "Expected an asset-root relative path.");
	}
	// Force Modified even when an explicit save preserves file size/timestamp.
	return ScanAssetDirectory(m_assetRoot, path.lexically_normal().generic_string(), outError);
}

bool AssetManager::NotifyAssetContentReplaced(const Guid& guid)
{
	const auto entry = m_catalog.find(guid);
	if (entry == m_catalog.end()) return false;
	const AssetChange change{ AssetChangeKind::Modified, entry->second.type, guid, entry->second.relativePath };
	const auto latest = std::find_if(m_pendingChanges.rbegin(), m_pendingChanges.rend(),
		[&](const AssetChange& existing)
		{
			return existing.guid == change.guid && existing.relativePath == change.relativePath;
		});
	if (latest == m_pendingChanges.rend() || *latest != change) m_pendingChanges.push_back(change);
	return true;
}

std::vector<AssetChange> AssetManager::TakePendingChanges()
{
	std::vector<AssetChange> changes;
	changes.swap(m_pendingChanges);
	return changes;
}

bool AssetManager::ScanAssetDirectory(const std::string& rootDir, const std::string& notifiedPath, AssetCatalogError* outError)
{
	if (outError) *outError = {};
	try
	{
		if (rootDir.empty() || !fs::is_directory(rootDir))
			return FailCatalog(outError, AssetCatalogErrorCode::IoError, rootDir, "Asset root is not an existing directory.");
		std::string newRoot = fs::absolute(rootDir).lexically_normal().string();
		decltype(m_catalog) catalog;
		decltype(m_pathToId) pathToId;
		decltype(m_fileStates) fileStates;
		std::vector<std::pair<std::string, Guid>> missingMetadata;
		std::vector<fs::path> files;
		for (const auto& file : fs::recursive_directory_iterator(newRoot))
		{
			if (file.is_regular_file() && DetermineAssetType(file.path().extension().string()) != AssetType::Unknown)
				files.push_back(file.path());
		}
		std::sort(files.begin(), files.end());
		for (const auto& path : files)
		{
			const auto relativePath = path.lexically_relative(newRoot).generic_string();
			Guid id;
			if (fs::exists(path.string() + ".meta"))
			{
				const auto existing = MetaFile::TryLoad(path.string());
				if (!existing)
					return FailCatalog(outError, AssetCatalogErrorCode::InvalidMetadata, relativePath + ".meta",
						"Existing metadata is invalid; its identity was not replaced.");
				id = *existing;
			}
			else
			{
				id = GuidGenerator::Generate();
				if (!id.IsValid()) return FailCatalog(outError, AssetCatalogErrorCode::InvalidMetadata, relativePath, "GUID generation failed.");
				missingMetadata.emplace_back(path.string(), id);
			}
			const auto duplicate = catalog.find(id);
			if (duplicate != catalog.end())
				return FailCatalog(outError, AssetCatalogErrorCode::DuplicateGuid, relativePath + ".meta",
					"GUID also belongs to " + duplicate->second.relativePath + "; catalog was not updated.");
			catalog.emplace(id, AssetEntry{ id, relativePath, DetermineAssetType(path.extension().string()) });
			pathToId.emplace(relativePath, id);
		}

		// Validate all existing identities before creating metadata for new assets.
		// Never overwrite an existing sidecar, including one created during the scan.
		for (const auto& [path, id] : missingMetadata)
		{
			if (!MetaFile::Save(path, id, MetaFile::WriteMode::CreateNew))
				return FailCatalog(outError, AssetCatalogErrorCode::IoError, path + ".meta", "Could not create new asset metadata.");
		}
		for (const auto& [id, entry] : catalog)
		{
			const fs::path path = fs::path(newRoot) / entry.relativePath;
			const fs::path meta = path.string() + ".meta";
			fileStates.emplace(id, FileState{ fs::last_write_time(path), fs::file_size(path),
				fs::last_write_time(meta), fs::file_size(meta) });
		}

		std::vector<AssetChange> changes;
		for (const auto& [id, old] : m_catalog)
		{
			if (!catalog.contains(id)) changes.push_back({ AssetChangeKind::Removed, old.type, id, old.relativePath });
		}
		for (const auto& [id, entry] : catalog)
		{
			const auto old = m_catalog.find(id);
			if (old == m_catalog.end()) changes.push_back({ AssetChangeKind::Added, entry.type, id, entry.relativePath });
			else if (old->second.relativePath != entry.relativePath || old->second.type != entry.type ||
				m_fileStates.at(id) != fileStates.at(id) || entry.relativePath == notifiedPath)
				changes.push_back({ AssetChangeKind::Modified, entry.type, id, entry.relativePath });
		}
		std::sort(changes.begin(), changes.end(), [](const auto& a, const auto& b)
		{
			if (a.relativePath != b.relativePath) return a.relativePath < b.relativePath;
			return a.kind < b.kind;
		});
		auto pending = m_pendingChanges;
		for (const auto& change : changes)
		{
			const auto latest = std::find_if(pending.rbegin(), pending.rend(), [&](const AssetChange& existing)
			{
				return existing.guid == change.guid && existing.relativePath == change.relativePath;
			});
			if (latest == pending.rend() || *latest != change) pending.push_back(change);
		}

		// Publish all catalog indices, scan observations and notifications together.
		m_assetRoot.swap(newRoot);
		m_catalog.swap(catalog);
		m_pathToId.swap(pathToId);
		m_fileStates.swap(fileStates);
		m_pendingChanges.swap(pending);
		return true;
	}
	catch (const std::exception& exception)
	{
		return FailCatalog(outError, AssetCatalogErrorCode::IoError, rootDir, exception.what());
	}
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
	if (ext == ".imprint") return AssetType::ActorImprint;
	if (ext == ".scene") return AssetType::Scene;

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

std::string AssetManager::GetAssetPath(const Guid& guid) const
{
	const AssetEntry* entry = GetAssetEntry(guid);
	return entry ? (fs::path(m_assetRoot) / entry->relativePath).string() : std::string{};
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
