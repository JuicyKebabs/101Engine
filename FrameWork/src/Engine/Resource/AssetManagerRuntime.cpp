#include "AssetManager.h"
#include "RenderAssetResources.h"
#include "BuiltinMeshAssets.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Graphics/RenderingConversions.h"
#include "Engine/Core/Debug/Debug.h"
#include "Renderer/Renderer.h"

#include <algorithm>
#include <stdexcept>

namespace
{

// Scene sets are small, and acquisition runs on cache misses/Component edits,
// not in a per-vertex loop. Linear identity deduplication avoids another handle
// hash policy while preserving owner/slot/generation equality.
template<class Value>
void Include(std::vector<Value>& values, const Value& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

template<class Value>
void IncludeAll(std::vector<Value>& destination, const std::vector<Value>& source)
{
    for (const Value& value : source)
    {
        Include(destination, value);
    }
}

void MergeUsage(SceneAssetUsage& destination, const SceneAssetUsage& source)
{
    IncludeAll(destination.geometries, source.geometries);
    IncludeAll(destination.textures, source.textures);
    IncludeAll(destination.materials, source.materials);
    IncludeAll(destination.newGeometries, source.newGeometries);
    IncludeAll(destination.newTextures, source.newTextures);
    IncludeAll(destination.newMaterials, source.newMaterials);
    IncludeAll(destination.modelKeys, source.modelKeys);
    IncludeAll(destination.textureKeys, source.textureKeys);
    IncludeAll(destination.dependencyTextureKeys, source.dependencyTextureKeys);
}

std::string Utf8PathKey(const std::filesystem::path& path)
{
    const std::u8string encoded = path.lexically_normal().generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

void CheckDeviceFailure(const rendering::Result& result)
{
    if (!result.IsSuccess() && result.failureKind != rendering::FailureKind::General)
    {
        // Scene preparation must not disguise a stopped Device as a missing file.
        throw std::runtime_error("Renderer device failure during Asset registration: " + result.message);
    }
}

}

// An acquisition stages cache nodes and a complete required set. Handles are
// tracked before later allocations can throw. Commit reserves all destination
// hash tables before transferring nodes, then swaps the Scene record without
// allocating. Destruction rolls back only this batch's new registrations.
class AssetManager::AcquisitionBatch
{
public:
    AcquisitionBatch(AssetManager& owner, const SceneBase& scene)
        : m_owner(owner), m_destination(owner.m_sceneUsage.try_emplace(&scene).first->second),
          m_usage(m_destination)
    {}

    ~AcquisitionBatch()
    {
        if (m_committed || !m_owner.m_renderer)
        {
            return;
        }

        for (auto handle : m_newMaterials)
        {
            const auto result = m_owner.m_renderer->DestroyMaterial(handle);
            ReportCleanup(result);
        }
        for (auto handle : m_newGeometries)
        {
            const auto result = m_owner.m_renderer->DestroyGeometry(handle);
            ReportCleanup(result);
        }
        for (auto handle : m_newTextures)
        {
            const auto result = m_owner.m_renderer->DestroyTexture(handle);
            ReportCleanup(result);
        }
    }

    void Commit()
    {
        // Reserving can change bucket capacity, but cannot publish a new value.
        // The standard Guid/string hash functions used here allocate no memory.
        m_owner.m_loadedModels.reserve(m_owner.m_loadedModels.size() + m_models.size());
        m_owner.m_loadedTextures.reserve(m_owner.m_loadedTextures.size() + m_textures.size());
        m_owner.m_dependencyTextures.reserve(m_owner.m_dependencyTextures.size() + m_dependencyTextures.size());
        m_owner.m_loadedModels.merge(m_models);
        m_owner.m_loadedTextures.merge(m_textures);
        m_owner.m_dependencyTextures.merge(m_dependencyTextures);
        std::swap(m_destination, m_usage);
        m_committed = true;
    }

    rendering::TextureHandle Texture(const Guid& guid)
    {
        Include(m_usage.textureKeys, guid);
        const auto cached = m_owner.m_loadedTextures.find(guid);
        if (cached != m_owner.m_loadedTextures.end())
        {
            RetainTexture(cached->second);
            return cached->second;
        }

        const auto staged = m_textures.find(guid);
        if (staged != m_textures.end())
        {
            return staged->second;
        }

        // Allocate the failed/success result slot before registering a resource.
        auto inserted = m_textures.try_emplace(guid);
        const AssetEntry* entry = m_owner.GetAssetEntry(guid);
        if (!entry || entry->type != AssetType::Texture)
        {
            DBG("Texture Asset missing: %s", guid.ToString().c_str());
            return inserted.first->second;
        }

        inserted.first->second = LoadTexture(std::filesystem::path(m_owner.m_assetRoot) / std::filesystem::u8path(entry->relativePath));
        return inserted.first->second;
    }

    const CachedModel& Model(const Guid& guid)
    {
        Include(m_usage.modelKeys, guid);
        const auto cached = m_owner.m_loadedModels.find(guid);
        if (cached != m_owner.m_loadedModels.end())
        {
            RetainModel(cached->second);
            return cached->second;
        }

        auto inserted = m_models.try_emplace(guid);
        CachedModel& result = inserted.first->second;
        const BuiltinMeshAsset* builtin = FindBuiltinMeshAsset(guid);
        if (builtin)
        {
            GeometryInfo geometry;
            geometry.texture = White();
            if (builtin->mesh == DefaultMesh::Quad && m_owner.m_renderAssets)
            {
                geometry.geometry = m_owner.m_renderAssets->Quad();
            }
            else
            {
                const ::Model generated = GetDefaultModel(builtin->mesh);
                if (!generated.empty())
                {
                    std::vector<rendering::MeshVertex> vertices;
                    vertices.reserve(generated.front().vertices.size());
                    for (const Vertex& source : generated.front().vertices)
                    {
                        rendering::MeshVertex vertex;
                        vertex.position = EngineRendering::ToRenderer(source.position);
                        vertex.normal = EngineRendering::ToRenderer(source.normal);
                        vertex.tangent = EngineRendering::ToRenderer(source.tangent);
                        vertex.uv.x = source.uv.x;
                        vertex.uv.y = source.uv.y;
                        vertex.color = EngineRendering::ToRenderer(source.color);
                        vertices.push_back(vertex);
                    }

                    rendering::GeometryDesc description;
                    description.vertices = vertices;
                    description.indices = generated.front().indices;
                    geometry.geometry = RegisterGeometry(description);
                }
            }

            result.placements.push_back(geometry);
            RetainModel(result);
            return result;
        }

        const AssetEntry* entry = m_owner.GetAssetEntry(guid);
        rendering::assets::ModelData imported;
        bool parsed = false;
        if (entry && entry->type == AssetType::Mesh)
        {
            const auto loaded = rendering::assets::LoadModel(std::filesystem::absolute(m_owner.m_assetRoot),
                std::filesystem::u8path(entry->relativePath), imported);
            parsed = loaded.IsSuccess();
            if (!parsed)
            {
                DBG("Model import: %s", loaded.message.c_str());
            }
        }

        if (!parsed || imported.placements.empty())
        {
            // A failed complete model remains an explicit cached Invalid element.
            // Retaining its key prevents same-Scene requests from retrying forever.
            GeometryInfo unavailable;
            result.placements.push_back(unavailable);
            return result;
        }

        std::vector<GeometryInfo> uniqueMeshes;
        uniqueMeshes.reserve(imported.meshes.size());
        result.registeredGeometries.reserve(imported.meshes.size());
        for (const rendering::assets::MeshData& mesh : imported.meshes)
        {
            GeometryInfo geometry;
            geometry.texture = mesh.texturePath.empty() ? White() : DependencyTexture(mesh.texturePath, result);
            if (mesh.valid)
            {
                const rendering::GeometryDesc description = mesh.Description();
                geometry.geometry = RegisterGeometry(description);
            }

            if (!mesh.diagnostic.empty())
            {
                DBG("SubMesh import: %s", mesh.diagnostic.c_str());
            }

            uniqueMeshes.push_back(geometry);
            result.registeredGeometries.push_back(geometry.geometry);
        }

        result.placements.reserve(imported.placements.size());
        for (const rendering::assets::MeshPlacement& placement : imported.placements)
        {
            GeometryInfo geometry;
            if (placement.meshIndex < uniqueMeshes.size())
            {
                geometry = uniqueMeshes[placement.meshIndex];
            }

            geometry.localToModel = EngineRendering::FromRenderer(placement.localToModel);
            result.placements.push_back(geometry);
        }

        result.animation.nodes = std::move(imported.nodes);
        result.animation.channels = std::move(imported.animationChannels);
        result.animation.duration = imported.animationDuration;
        result.animation.ticksPerSecond = imported.ticksPerSecond;
        RetainModel(result);
        return result;
    }

    rendering::MaterialHandle Material(const rendering::MaterialDesc& description)
    {
        rendering::MaterialHandle unavailable;
        if (!m_owner.m_renderer)
        {
            return unavailable;
        }

        m_newMaterials.reserve(m_newMaterials.size() + 1);
        m_usage.materials.reserve(m_usage.materials.size() + 1);
        m_usage.newMaterials.reserve(m_usage.newMaterials.size() + 1);
        auto registration = m_owner.m_renderer->RegisterMaterial(description);
        CheckDeviceFailure(registration.result);
        if (!registration.result.IsSuccess())
        {
            DBG("Material registration: %s", registration.result.message.c_str());
            return unavailable;
        }

        m_newMaterials.push_back(registration.handle);
        m_usage.materials.push_back(registration.handle);
        m_usage.newMaterials.push_back(registration.handle);
        return registration.handle;
    }

private:
    void ReportCleanup(const rendering::Result& result) const noexcept
    {
        if (!result.IsSuccess())
        {
            // A stopped Renderer still owns the allocation until its fatal
            // shutdown policy runs; never claim physical cleanup succeeded.
            DBG("Asset acquisition rollback: %s", result.message.c_str());
        }
    }

    rendering::TextureHandle White() const noexcept
    {
        if (m_owner.m_renderAssets)
        {
            return m_owner.m_renderAssets->White();
        }

        rendering::TextureHandle unavailable;
        return unavailable;
    }

    void RetainTexture(rendering::TextureHandle handle)
    {
        if (handle != rendering::TextureHandle{} && handle != White())
        {
            Include(m_usage.textures, handle);
        }
    }

    void RetainModel(const CachedModel& model)
    {
        // A cache hit inherits every successful registration, including inputs
        // not used by a visible placement. Keeping only cache keys would leave a
        // cache entry pointing at a Destroy-requested Texture after Scene A ends
        // while Scene B still retains the model key.
        for (auto handle : model.registeredGeometries)
        {
            if (handle != rendering::GeometryHandle{})
            {
                Include(m_usage.geometries, handle);
            }
        }

        for (const GeometryInfo& geometry : model.placements)
        {
            const bool residentQuad = m_owner.m_renderAssets && geometry.geometry == m_owner.m_renderAssets->Quad();
            if (geometry.geometry != rendering::GeometryHandle{} && !residentQuad)
            {
                Include(m_usage.geometries, geometry.geometry);
            }

            RetainTexture(geometry.texture);
        }

        IncludeAll(m_usage.dependencyTextureKeys, model.textureDependencies);
        IncludeAll(m_usage.textureKeys, model.textureAssetDependencies);

        for (const Guid& key : model.textureAssetDependencies)
        {
            const auto cached = m_owner.m_loadedTextures.find(key);
            if (cached != m_owner.m_loadedTextures.end())
            {
                RetainTexture(cached->second);
                continue;
            }

            const auto staged = m_textures.find(key);
            if (staged == m_textures.end())
            {
                throw std::logic_error("Retained model Texture GUID has no cached result.");
            }

            RetainTexture(staged->second);
        }

        for (const std::string& key : model.textureDependencies)
        {
            const auto cached = m_owner.m_dependencyTextures.find(key);
            if (cached != m_owner.m_dependencyTextures.end())
            {
                RetainTexture(cached->second);
                continue;
            }

            const auto staged = m_dependencyTextures.find(key);
            if (staged == m_dependencyTextures.end())
            {
                throw std::logic_error("Retained model Texture path has no cached result.");
            }

            RetainTexture(staged->second);
        }
    }

    rendering::GeometryHandle RegisterGeometry(const rendering::GeometryDesc& description)
    {
        rendering::GeometryHandle unavailable;
        if (!m_owner.m_renderer)
        {
            return unavailable;
        }

        m_newGeometries.reserve(m_newGeometries.size() + 1);
        m_usage.geometries.reserve(m_usage.geometries.size() + 1);
        m_usage.newGeometries.reserve(m_usage.newGeometries.size() + 1);
        auto registration = m_owner.m_renderer->RegisterGeometry(description);
        CheckDeviceFailure(registration.status);
        if (!registration.status.IsSuccess())
        {
            DBG("Geometry registration: %s", registration.status.message.c_str());
            return unavailable;
        }

        m_newGeometries.push_back(registration.handle);
        m_usage.geometries.push_back(registration.handle);
        m_usage.newGeometries.push_back(registration.handle);
        return registration.handle;
    }

    rendering::TextureHandle LoadTexture(const std::filesystem::path& file)
    {
        rendering::TextureHandle unavailable;
        rendering::assets::TextureData decoded;
        const auto loaded = rendering::assets::LoadTexture(std::filesystem::absolute(m_owner.m_assetRoot), file, decoded);
        if (!loaded.IsSuccess())
        {
            DBG("Texture import: %s", loaded.message.c_str());
            return unavailable;
        }

        if (!m_owner.m_renderer)
        {
            return unavailable;
        }

        std::vector<rendering::TextureMipDesc> mipViews;
        rendering::TextureDesc description;
        decoded.Description(description, mipViews);
        m_newTextures.reserve(m_newTextures.size() + 1);
        m_usage.textures.reserve(m_usage.textures.size() + 1);
        m_usage.newTextures.reserve(m_usage.newTextures.size() + 1);
        auto registration = m_owner.m_renderer->RegisterTexture(description);
        CheckDeviceFailure(registration.status);
        if (!registration.status.IsSuccess())
        {
            DBG("Texture registration: %s", registration.status.message.c_str());
            return unavailable;
        }

        m_newTextures.push_back(registration.handle);
        m_usage.textures.push_back(registration.handle);
        m_usage.newTextures.push_back(registration.handle);
        return registration.handle;
    }

    rendering::TextureHandle DependencyTexture(const std::filesystem::path& file, CachedModel& model)
    {
        const std::filesystem::path relative = file.lexically_relative(std::filesystem::absolute(m_owner.m_assetRoot));
        const AssetEntry* catalogTexture = m_owner.GetAssetEntryByPath(Utf8PathKey(relative));
        if (catalogTexture && catalogTexture->type == AssetType::Texture)
        {
            Include(model.textureAssetDependencies, catalogTexture->guid);
            return Texture(catalogTexture->guid);
        }

        const std::string key = Utf8PathKey(file);
        Include(model.textureDependencies, key);
        Include(m_usage.dependencyTextureKeys, key);
        const auto cached = m_owner.m_dependencyTextures.find(key);
        if (cached != m_owner.m_dependencyTextures.end())
        {
            RetainTexture(cached->second);
            return cached->second;
        }

        const auto staged = m_dependencyTextures.find(key);
        if (staged != m_dependencyTextures.end())
        {
            return staged->second;
        }

        auto inserted = m_dependencyTextures.try_emplace(key);
        inserted.first->second = LoadTexture(file);
        return inserted.first->second;
    }

    AssetManager& m_owner;
    SceneAssetUsage& m_destination;
    SceneAssetUsage m_usage;
    std::unordered_map<Guid, CachedModel> m_models;
    std::unordered_map<Guid, rendering::TextureHandle> m_textures;
    std::unordered_map<std::string, rendering::TextureHandle> m_dependencyTextures;
    std::vector<rendering::GeometryHandle> m_newGeometries;
    std::vector<rendering::TextureHandle> m_newTextures;
    std::vector<rendering::MaterialHandle> m_newMaterials;
    bool m_committed = false;
};

AssetManager::~AssetManager()
{
    while (!m_sceneUsage.empty())
    {
        ReleaseScene(m_sceneUsage.begin()->first);
    }
}

const std::vector<GeometryInfo>& AssetManager::GetGeometryInfos(SceneBase& scene, const Guid& guid)
{
    AcquisitionBatch batch(*this, scene);
    batch.Model(guid);
    batch.Commit();
    return m_loadedModels.at(guid).placements;
}

rendering::TextureHandle AssetManager::GetTextureHandle(SceneBase& scene, const Guid& guid)
{
    AcquisitionBatch batch(*this, scene);
    const rendering::TextureHandle handle = batch.Texture(guid);
    batch.Commit();
    return handle;
}

rendering::MaterialHandle AssetManager::RegisterSceneMaterial(SceneBase& scene, const rendering::MaterialDesc& description)
{
    AcquisitionBatch batch(*this, scene);
    const rendering::MaterialHandle handle = batch.Material(description);
    batch.Commit();
    return handle;
}

const ModelAnimationMetadata* AssetManager::GetModelAnimationMetadata(const Guid& guid) const
{
    const auto cached = m_loadedModels.find(guid);
    return cached == m_loadedModels.end() ? nullptr : &cached->second.animation;
}

void AssetManager::ReleaseScene(const SceneBase* scene) noexcept
{
    const auto released = m_sceneUsage.find(scene);
    if (released == m_sceneUsage.end())
    {
        return;
    }

    SceneAssetUsage ending;
    std::swap(ending, released->second);
    m_sceneUsage.erase(released);
    ReleaseUnusedUsage(ending);
}

void AssetManager::ReleaseUnusedUsage(SceneAssetUsage& ending) noexcept
{
    const auto stillRequired = [&](const auto& value, auto member)
    {
        return std::any_of(m_sceneUsage.begin(), m_sceneUsage.end(), [&](const auto& record)
        {
            const auto& values = record.second.*member;
            return std::find(values.begin(), values.end(), value) != values.end();
        });
    };

    if (m_renderer)
    {
        for (auto handle : ending.materials)
        {
            if (!stillRequired(handle, &SceneAssetUsage::materials))
            {
                const auto result = m_renderer->DestroyMaterial(handle);
                if (!result.IsSuccess())
                {
                    DBG("Scene Material destruction: %s", result.message.c_str());
                }
            }
        }
        for (auto handle : ending.geometries)
        {
            if (!stillRequired(handle, &SceneAssetUsage::geometries))
            {
                const auto result = m_renderer->DestroyGeometry(handle);
                if (!result.IsSuccess())
                {
                    DBG("Scene Geometry destruction: %s", result.message.c_str());
                }
            }
        }
        for (auto handle : ending.textures)
        {
            if (!stillRequired(handle, &SceneAssetUsage::textures))
            {
                const auto result = m_renderer->DestroyTexture(handle);
                if (!result.IsSuccess())
                {
                    DBG("Scene Texture destruction: %s", result.message.c_str());
                }
            }
        }
    }

    // Keys matter independently from handles: a wholly failed Asset has no GPU
    // identity, but must remain cached until its final Scene owner goes away.
    std::erase_if(m_loadedModels, [&](const auto& entry) { return !stillRequired(entry.first, &SceneAssetUsage::modelKeys); });
    std::erase_if(m_loadedTextures, [&](const auto& entry) { return !stillRequired(entry.first, &SceneAssetUsage::textureKeys); });
    std::erase_if(m_dependencyTextures, [&](const auto& entry) { return !stillRequired(entry.first, &SceneAssetUsage::dependencyTextureKeys); });
}

std::unique_ptr<PreparedSceneAssetTransfer> AssetManager::PrepareSceneTransfer(
    const SceneBase& source, const SceneBase& destination)
{
    auto prepared = std::make_unique<PreparedSceneAssetTransfer>();
    prepared->source = &source;
    prepared->destination = &destination;
    auto destinationRecord = m_sceneUsage.try_emplace(&destination);
    const auto sourceRecord = m_sceneUsage.find(&source);
    prepared->combined = destinationRecord.first->second;
    if (sourceRecord != m_sceneUsage.end())
    {
        MergeUsage(prepared->combined, sourceRecord->second);
    }

    return prepared;
}

void AssetManager::CommitSceneTransfer(PreparedSceneAssetTransfer& transfer) noexcept
{
    const auto destination = m_sceneUsage.find(transfer.destination);
    if (destination == m_sceneUsage.end())
    {
        std::terminate();
    }

    std::swap(destination->second, transfer.combined);
    m_sceneUsage.erase(transfer.source);
}

SceneAssetAcquisition::SceneAssetAcquisition(AssetManager& owner, const SceneBase& scene)
    : m_owner(owner), m_scene(&scene), m_previous(owner.m_sceneUsage.try_emplace(&scene).first->second)
{
}

SceneAssetAcquisition::~SceneAssetAcquisition()
{
    if (m_committed)
    {
        return;
    }

    const auto current = m_owner.m_sceneUsage.find(m_scene);
    if (current == m_owner.m_sceneUsage.end())
    {
        std::terminate();
    }

    SceneAssetUsage acquired;
    std::swap(acquired, current->second);
    std::swap(current->second, m_previous);
    // Required sets now describe the previous live state. Only newly unneeded
    // registrations/cache keys are released; other Scenes retain their union.
    m_owner.ReleaseUnusedUsage(acquired);
}
