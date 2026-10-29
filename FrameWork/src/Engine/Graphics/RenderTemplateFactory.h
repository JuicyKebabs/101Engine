#pragma once
#include "Engine/Graphics/RenderData.h"
#include "Engine/Resource/MeshHandle.h"
#include "Engine/Resource/Texture.h"
#include "Renderer/DrawTypes.h"
#include <vector>

// Each Component owns its CPU draw template and Bounds snapshot. These values
// neither retain GPU objects nor duplicate the AssetManager's GUID cache.
struct MeshDesc
{
    MeshHandle meshHandle;
    std::uint32_t startIndex = 0;
    std::int32_t baseVertex = 0;
    Vector3 boundsCenter;
    float boundsRadius = 0;
    std::uint64_t boundsRevision = 0;
};

// The automatic Material has fixed shader/blend intent. Its owned description
// lets a Component update its own UV/pivot/flip without editing a shared value.
struct ComponentMaterialState
{
    TextureHandle textureHandle;
    rendering::MaterialHandle handle;
    rendering::MaterialDesc description;
    bool screen = false;
};

struct SubmeshRenderTemplate
{
    MeshDesc meshDesc;
    ComponentMaterialState materialDesc;
    Matrix4x4 localToModel = Matrix4x4::Identity();
};
using MeshRenderTemplate = std::vector<SubmeshRenderTemplate>;

struct SpriteRenderTemplate
{
    ComponentMaterialState materialDesc;
    BillboardType billboardType = BillboardType::None;
};

struct UIRenderElement
{
    ComponentMaterialState materialDesc;
};
using UIRenderTemplate = std::vector<UIRenderElement>;
