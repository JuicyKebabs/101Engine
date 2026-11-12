#pragma once

#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetType.h"
#include "Engine/Resource/AssetChange.h"
#include "Engine/Resource/SceneAssetUsage.h"
#include "RendererAssets/AssetFiles.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Audio/AudioHandle.h"
#include "Engine/Resource/AnimationClip.h"

#include <memory>
#include <optional>
#include <span>
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
	rendering::GeometryHandle geometry;				// Handle to the geometry resource in renderer.
	rendering::TextureHandle texture;				// Handle to the texture resource in renderer.
	Matrix4x4 localToModel = Matrix4x4::Identity();	// Local transform from the node to the model space.
	std::optional<std::size_t> meshIndex;			// Index of the mesh in the model's mesh array, if applicable.
};

struct SkinnedModelView
{
	std::span<const GeometryInfo> placements;					// View of the geometry placements for the model.
	std::span<const rendering::assets::ModelNode> nodes;		// View of the model's node hierarchy.
	std::span<const rendering::assets::MeshSkinData> meshSkins;	// View of the skinning data for each mesh in the model.
	std::uint64_t revision = 0;									// Incremented when cached geometry is released and re-registered.
};

struct ModelAnimationMetadata
{
	std::string name;											// Name of the animation clip.
	std::vector<rendering::assets::AnimationChannel> channels;	// Animation channels for the clip.
	double duration = 0;										// Duration of the animation clip in seconds.
	double ticksPerSecond = 30;									// Ticks per second for the animation clip.
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

	static AssetType DetermineAssetType(const std::string& extension);

	// Look up funcctions.
	const AssetEntry* GetAssetEntryByPath(const std::string& relativePath) const;
	const AssetEntry* GetAssetEntry(const Guid& guid) const;
	std::vector<AssetEntry> GetAssetEntries(AssetType type) const;
	std::string GetAssetPath(const Guid& guid) const;
	const std::string& GetAssetRoot() const { return m_assetRoot; }
	const std::vector<GeometryInfo>& GetGeometryInfos(SceneBase& scene, const Guid& guid);
	SkinnedModelView GetSkinnedModel(SceneBase& scene, const Guid& guid);
	rendering::TextureHandle GetTextureHandle(SceneBase& scene, const Guid& guid);
	AudioHandle GetAudioHandle(const Guid& guid);
	AudioHandle GetAudioHandleByPath(const std::string& relativePath);

	std::optional<SkinnedModelView> FindLoadedSkinnedModel(const Guid& guid) const;
	std::optional<AnimationClipHandle> LoadAnimationClip(const Guid& guid);
	const AnimationClip* FindAnimationClip(AnimationClipHandle handle) const;

	rendering::MaterialHandle RegisterSceneMaterial(SceneBase& scene, const rendering::MaterialDesc& description);
	void ReleaseScene(const SceneBase* scene) noexcept;
	std::unique_ptr<PreparedSceneAssetTransfer> PrepareSceneTransfer(const SceneBase& source, const SceneBase& destination);
	void CommitSceneTransfer(PreparedSceneAssetTransfer& transfer) noexcept;

private:
	// Catalog of all assets in the asset root directory, indexed by GUID and relative path.
	std::unordered_map<Guid, AssetEntry> m_catalog;		// GUID -> Entry (Subscribe without loading)
	std::unordered_map<std::string, Guid> m_pathToId;	// Path -> GUID (Reverse lookup)

	std::string m_assetRoot;	// Root directory for assets

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

	//------------------------------------------------------------------------------------
	// Model cache (actual data is managed by Renderer)
	// CachedModel contains the datas which constract the model and the revision number.
	// A model is consisted of geometries which shape the model,
	// and the datas which describe the model's node hierarchy and skinning data.
	// This also has the information about texture dependencies of the model,
	// which is used to track the texture assets described in the source model file.
	//------------------------------------------------------------------------------------
	struct CachedModel
	{
		std::vector<GeometryInfo> placements;
		std::vector<rendering::GeometryHandle> registeredGeometries;
		std::vector<rendering::assets::ModelNode> nodes;
		std::vector<rendering::assets::MeshSkinData> meshSkins;
		std::vector<ModelAnimationMetadata> animation;
		std::vector<std::string> textureDependencies;
		std::vector<Guid> textureAssetDependencies;
		std::uint64_t revision = 0;
	};

	std::unordered_map<Guid, CachedModel> m_loadedModels;
	std::uint64_t m_nextModelRevision = 1;

	//------------------------------------------------------------------------------------------------
	// Texture cache (actual data is managed by Renderer)
	//------------------------------------------------------------------------------------------------
	std::unordered_map<Guid, rendering::TextureHandle> m_loadedTextures;
	std::unordered_map<std::string, rendering::TextureHandle> m_dependencyTextures;

	//------------------------------------------------------------------------------------------------
	// Audio cache. (actual data is managed by AudioManager)
	//------------------------------------------------------------------------------------------------
	std::unordered_map<Guid, AudioHandle> m_loadedAudio;

	//------------------------------------------------------------------------------------------------
	// Animation clip cache.
	// Assetmanager stores the loaded animation clips and provides handles to access them.
	//------------------------------------------------------------------------------------------------
	struct CachedAnimationClip
	{
		Guid guid;
		std::unique_ptr<AnimationClip> clip;
		std::uint32_t generation = 0;
	};
	std::vector<CachedAnimationClip> m_animationClipCache;
	std::unordered_map<Guid, AnimationClipHandle> m_loadedAnimationClips;

	//------------------------------------------------------------------------------------------------
	// Scene usage tracking. 
	// Each SceneBase instance that has registered assets is tracked here.
	//------------------------------------------------------------------------------------------------
	std::unordered_map<const SceneBase*, SceneAssetUsage> m_sceneUsage;

	// References to necessary engine systems for asset loading
	rendering::Renderer* m_renderer = nullptr;
	RenderAssetResources* m_renderAssets = nullptr;
	AudioManager* m_pAudioManager = nullptr;

	class AcquisitionBatch;

private:
	friend class SceneAssetAcquisition;
	void ReleaseUnusedUsage(SceneAssetUsage& ending) noexcept;

	// Scans root directory recursively and creates or resolves a GUID for each asset file via .meta sidecar file.
	// No asset is loaded at this time, only the catalog is built.
	bool ScanAssetDirectory(const std::string& rootDir, const std::string& notifiedPath, AssetCatalogError* outError);
};
