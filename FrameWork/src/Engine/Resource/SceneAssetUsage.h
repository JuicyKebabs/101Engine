#pragma once

#include "Engine/Core/GUID/Guid.h"
#include "Renderer/ResourceHandle.h"

#include <string>
#include <vector>

class SceneBase;
class AssetManager;

// Engine-side logical retention, never native GPU ownership or Handle refcount.
// Required resources include cache hits; new registrations are separately kept
// for preparation rollback and diagnostics. Entries intentionally last until
// Scene teardown, rather than being removed when one Component disappears.
struct SceneAssetUsage
{
    std::vector<rendering::GeometryHandle> geometries;
    std::vector<rendering::TextureHandle> textures;
    std::vector<rendering::MaterialHandle> materials;
    std::vector<rendering::GeometryHandle> newGeometries;
    std::vector<rendering::TextureHandle> newTextures;
    std::vector<rendering::MaterialHandle> newMaterials;
    std::vector<Guid> modelKeys;
    std::vector<Guid> textureKeys;
    std::vector<std::string> dependencyTextureKeys;
};

// Actor transactions prepare this value while failure is still recoverable.
// Commit exchanges complete records without allocation before Actors publish.
struct PreparedSceneAssetTransfer
{
    const SceneBase* source = nullptr;
    const SceneBase* destination = nullptr;
    SceneAssetUsage combined;
};

// One Component preparation may load a model, register several Materials, then
// fail while retaining its CPU templates. This outer guard covers that whole
// operation in addition to each individual AssetManager registration batch.
class SceneAssetAcquisition
{
public:
    SceneAssetAcquisition(AssetManager& owner, const SceneBase& scene);
    ~SceneAssetAcquisition();
    SceneAssetAcquisition(const SceneAssetAcquisition&) = delete;
    SceneAssetAcquisition& operator=(const SceneAssetAcquisition&) = delete;
    void Commit() noexcept { m_committed = true; }

private:
    AssetManager& m_owner;
    const SceneBase* m_scene = nullptr;
    SceneAssetUsage m_previous;
    bool m_committed = false;
};
