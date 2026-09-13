#pragma once
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetType.h"
#include "Engine/Resource/AssetChange.h"
#include "Engine/Resource/MeshHandle.h"
#include "Engine/Resource/Texture.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>

//------------------------------------------------------------------------------------
// AssetManager class
// Scans and stores all asset information in the project directory.
// Scan all asset files and create .meta file if it doesn't exist.
// Load meta file and store the asset information in catalog(not loaded at that time).
// Provide functions to load asset when it gets requested.
//------------------------------------------------------------------------------------

// Forward declarations
class TextureManager;
class MeshManager;

struct AssetEntry
{
	Guid guid;								// Unique identifier for the asset
	std::string relativePath;				// Relative path to the asset file
	AssetType type = AssetType::Unknown;	// Type of the asset
};

enum class AssetCatalogErrorCode { None, IoError, InvalidMetadata, DuplicateGuid, InvalidPath };

struct AssetCatalogError
{
	AssetCatalogErrorCode code = AssetCatalogErrorCode::None;
	std::string path;
	std::string message;
};

class AssetManager
{
public:
	AssetManager() = default;
	~AssetManager() = default;

	// Initialize the AssetManager with the project directory
	bool Initialize(
		const std::string& projectDir,
		TextureManager* pTextureManager,
		MeshManager* pMeshManager,
		AssetCatalogError* outError = nullptr
	);

	// Transactional catalog refresh. Failure preserves catalog and pending events.
	bool Refresh(AssetCatalogError* outError = nullptr);
	bool NotifyAssetChanged(const std::string& relativePath, AssetCatalogError* outError = nullptr);
	// Queues a content-only change for an identity already validated by this
	// catalog. Editor atomic saves use this after the filesystem commit; no
	// whole-catalog rescan can make that commit appear to fail afterward.
	bool NotifyAssetContentReplaced(const Guid& guid);
	std::vector<AssetChange> TakePendingChanges();

	// Lookup functions to retrieve asset entries by path or GUID
	const AssetEntry* GetAssetEntryByPath(const std::string& relativePath) const;
	const AssetEntry* GetAssetEntry(const Guid& guid) const;
	std::vector<AssetEntry> GetAssetEntries(AssetType type) const;
	std::string GetAssetPath(const Guid& guid) const;
	const std::string& GetAssetRoot() const { return m_assetRoot; }

	// Get the handle for each asset type by GUID
	MeshHandle GetMeshHandle(const Guid& guid);
	TextureHandle GetTextureHandle(const Guid& guid);

private:
	std::unordered_map<Guid, AssetEntry> m_catalog;		// GUID -> Entry (Subscribe without loading)
	std::unordered_map<std::string, Guid> m_pathToId;	// Path -> GUID (Reverse lookup)
	struct FileState
	{
		std::filesystem::file_time_type writeTime;
		std::uintmax_t size;
		std::filesystem::file_time_type metaWriteTime;
		std::uintmax_t metaSize;
		friend bool operator==(const FileState&, const FileState&) = default;
	};
	std::unordered_map<Guid, FileState> m_fileStates;
	std::vector<AssetChange> m_pendingChanges;

	// Lazy-load cache for each asset type
	std::unordered_map<Guid, MeshHandle>    m_loadedMeshes;	// Lazy-load cache
	std::unordered_map<Guid, TextureHandle> m_loadedTextures;	// Lazy-load cache

	std::string m_assetRoot;	// Root directory for assets

	// References to necessary engine systems for asset loading
	TextureManager* m_pTextureManager = nullptr;
	MeshManager* m_pMeshManager = nullptr;

private:
	// Scans root directory recursively and creates or resolves a GUID
	// for each asset file via .meta sidecar file.
	// No asset is loaded at this time, only the catalog is built.
	bool ScanAssetDirectory(const std::string& rootDir, const std::string& notifiedPath, AssetCatalogError* outError);

	// Deteremine asset type based on the file extension.
	AssetType DetermineAssetType(const std::string& extension);
};
