#pragma once
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetType.h"
#include "Engine/Resource/AssetChange.h"
#include "Engine/Resource/SceneAssetUsage.h"
#include "RendererAssets/AssetFiles.h"
#include "Engine/Core/Math/Math.h"
#include <memory>
#include "Engine/Resource/Texture.h"
#include "Engine/Audio/AudioHandle.h"
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
class RenderAssetResources;
class SceneBase;
namespace rendering { class Renderer; }
class AudioManager;

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

// One entry per model placement; repeated nodes may share GeometryHandle.
// Bounds/Revision snapshots belong to MeshRenderer, never this catalog cache.
struct GeometryInfo
{
    rendering::GeometryHandle geometry;
    rendering::TextureHandle texture;
    Matrix4x4 localToModel = Matrix4x4::Identity();
};

struct ModelAnimationMetadata
{
    std::vector<rendering::assets::ModelNode> nodes;
    std::vector<rendering::assets::AnimationChannel> channels;
    double duration = 0;
    double ticksPerSecond = 30;
};

class AssetManager
{
public:
	AssetManager() = default;
	~AssetManager();

	// Initialize the AssetManager with the project directory
	bool Initialize(
		const std::string& projectDir,
		rendering::Renderer* renderer,
        RenderAssetResources* renderAssets,
		AssetCatalogError* outError = nullptr
	);
	bool Initialize(
		const std::string& projectDir,
		rendering::Renderer* renderer,
        RenderAssetResources* renderAssets,
		AudioManager* pAudioManager,
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
    const std::vector<GeometryInfo>& GetGeometryInfos(SceneBase& scene, const Guid& guid);
    rendering::TextureHandle GetTextureHandle(SceneBase& scene, const Guid& guid);
    const ModelAnimationMetadata* GetModelAnimationMetadata(const Guid& guid) const;
    rendering::MaterialHandle RegisterSceneMaterial(SceneBase& scene, const rendering::MaterialDesc& description);
    void ReleaseScene(const SceneBase* scene) noexcept;
    std::unique_ptr<PreparedSceneAssetTransfer> PrepareSceneTransfer(const SceneBase& source, const SceneBase& destination);
    void CommitSceneTransfer(PreparedSceneAssetTransfer& transfer) noexcept;
	AudioHandle GetAudioHandle(const Guid& guid);
	AudioHandle GetAudioHandleByPath(const std::string& relativePath);

private:
    friend class SceneAssetAcquisition;
    void ReleaseUnusedUsage(SceneAssetUsage& ending) noexcept;
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
    struct CachedModel
    {
        std::vector<GeometryInfo> placements;
        // Retention also covers imported meshes with no visible node placement.
        // This is identity bookkeeping only; Bounds/Revision stay in Components.
        std::vector<rendering::GeometryHandle> registeredGeometries;
        ModelAnimationMetadata animation;
        std::vector<std::string> textureDependencies;
        std::vector<Guid> textureAssetDependencies;
    };
    class AcquisitionBatch;
    std::unordered_map<Guid, CachedModel> m_loadedModels;
    std::unordered_map<Guid, rendering::TextureHandle> m_loadedTextures;
    std::unordered_map<std::string, rendering::TextureHandle> m_dependencyTextures;
    std::unordered_map<const SceneBase*, SceneAssetUsage> m_sceneUsage;
	std::unordered_map<Guid, AudioHandle>   m_loadedAudio;		// Lazy-load cache

	std::string m_assetRoot;	// Root directory for assets

	// References to necessary engine systems for asset loading
    rendering::Renderer* m_renderer = nullptr;
    RenderAssetResources* m_renderAssets = nullptr;
	AudioManager* m_pAudioManager = nullptr;

private:
	// Scans root directory recursively and creates or resolves a GUID
	// for each asset file via .meta sidecar file.
	// No asset is loaded at this time, only the catalog is built.
	bool ScanAssetDirectory(const std::string& rootDir, const std::string& notifiedPath, AssetCatalogError* outError);

	// Deteremine asset type based on the file extension.
	AssetType DetermineAssetType(const std::string& extension);
};
