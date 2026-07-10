#pragma once
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/MeshHandle.h"
#include "Engine/Resource/Texture.h"
#include <unordered_map>
#include <string>
#include <vector>

//------------------------------------------------------------------------------------
// AssetManager class
// Scans and stores all asset information in the project directory.
// Scan all asset files and create .meta file if it doesn't exist.
// Load meta file and store the asset information in catalog(not loaded at that time).
// Provide functions to load asset when it gets requested.
//------------------------------------------------------------------------------------

enum class AssetType
{
	Mesh,
	Texture,
	//Material,
	//Shader,
	//Audio,
	//Font,
	//Animation,
	//Scene,
	//Script,
	Unknown
};

struct AssetEntry
{
	Guid guid;								// Unique identifier for the asset
	std::string relativePath;				// Relative path to the asset file
	AssetType type = AssetType::Unknown;	// Type of the asset
};

class AssetManager
{
public:
	static AssetManager& GetInstance()
	{
		static AssetManager instance;
		return instance;
	}

	// Initialize the AssetManager with the project directory
	void Initialize(const std::string& projectDir);

	// Lookup functions to retrieve asset entries by path or GUID
	const AssetEntry* GetAssetEntryByPath(const std::string& relativePath) const;
	const AssetEntry* GetAssetEntry(const Guid& guid) const;

	// Get the handle for each asset type by GUID
	MeshHandle GetMeshHandle(const Guid& guid);
	TextureHandle GetTextureHandle(const Guid& guid);

private:
	AssetManager() = default;

	// Scans root directory recursively and creates or resolves a GUID
	// for each asset file via .meta sidecar file.
	// No asset is loaded at this time, only the catalog is built.
	void ScanAssetDirectory(const std::string& rootDir);

	// Resolve the GUID for a given path.
	// Prepare new .meta file if it doesn't exist, otherwise load the existing .meta file.
	Guid ResolveGuidForPath(const std::string& filePath);

	// Deteremine asset type based on the file extension.
	AssetType DetermineAssetType(const std::string& extension);

	std::unordered_map<Guid, AssetEntry> m_catalog;		// GUID -> Entry (Subscribe without loading)
	std::unordered_map<std::string, Guid> m_pathToId;	// Path -> GUID (Reverse lookup)

	// Lazy-load cache for each asset type
	std::unordered_map<Guid, MeshHandle>    m_loadedMeshes;	// Lazy-load cache
	std::unordered_map<Guid, TextureHandle> m_loadedTextures;	// Lazy-load cache

	std::string m_assetRoot;	// Root directory for assets
};