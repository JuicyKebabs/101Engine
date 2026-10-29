#include "Engine/Actor/ActorHierarchyActivity.h"
#include "RenderSystem.h"
#include "RenderingConversions.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/UI/UIRenderer.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Resource/RenderAssetResources.h"
#include "Engine/Core/Debug/Debug.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

template<class ComponentType>
void RegisterOnce(std::vector<ComponentType*>& values, ComponentType* component)
{
    if (component && std::find(values.begin(), values.end(), component) == values.end())
    {
        values.push_back(component);
    }
}

template<class ComponentType>
void UnregisterValue(std::vector<ComponentType*>& values, ComponentType* component)
{
    std::erase(values, component);
}

enum class SourceKind { Mesh, Sprite, UI };

struct SubmissionSource
{
    RendererComponent* component = nullptr;
    SourceKind kind = SourceKind::Mesh;
    bool screen = false;
    std::vector<std::uint32_t> orderPath;
};

rendering::Vector4 LocalQuadTransform(const ComponentMaterialState& material)
{
    for (const auto& parameter : material.description.parameters)
    {
        if (parameter.name == "LocalQuadTransform")
        {
            if (const auto* value = std::get_if<rendering::Vector4>(&parameter.value))
            {
                return *value;
            }
        }
    }

    rendering::Vector4 identity;
    identity.x = 1;
    identity.y = 1;
    return identity;
}

// Frobenius norm bounds the maximum stretch, including shear. For Billboard,
// retain a sphere around the Actor origin so rotating an off-center pivot for
// Camera A/B cannot move Geometry outside a Camera-independent submitted bound.
void SetWorldBounds(rendering::RenderObject& object, Vector3 localCenter, float localRadius,
    std::uint64_t revision, const Matrix4x4& world)
{
    double stretchSquared = 0;
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            const double value = world.m[row][column];
            stretchSquared += value * value;
        }
    }

    Vector3 center;
    double radius = localRadius;
    if (object.billboard == rendering::BillboardMode::None)
    {
        center = world.TransformPoint(localCenter);
    }
    else
    {
        center = world.GetTranslation();
        radius += localCenter.Length();
    }

    radius *= std::sqrt(stretchSquared);
    if (!std::isfinite(radius) || radius > (std::numeric_limits<float>::max)())
    {
        return; // Missing Bounds safely disables culling; do not submit infinity.
    }

    rendering::BoundingSphere bounds;
    bounds.center = EngineRendering::ToRenderer(center);
    bounds.radius = std::nextafter(static_cast<float>(radius), (std::numeric_limits<float>::infinity)());
    if (!std::isfinite(bounds.radius))
    {
        return;
    }

    object.bounds = bounds;
    object.boundsGeometryRevision = revision;
}

}

void RenderSystem::Register(MeshRenderer* value) { RegisterOnce(m_meshRenderers, value); }
void RenderSystem::Register(SpriteRenderer* value) { RegisterOnce(m_spriteRenderers, value); }
void RenderSystem::Register(UIRenderer* value) { RegisterOnce(m_uiRenderers, value); }
void RenderSystem::Unregister(MeshRenderer* value) { UnregisterValue(m_meshRenderers, value); }
void RenderSystem::Unregister(SpriteRenderer* value) { UnregisterValue(m_spriteRenderers, value); }
void RenderSystem::Unregister(UIRenderer* value) { UnregisterValue(m_uiRenderers, value); }

void RenderSystem::PrepareRegistrationCapacity(std::size_t additionalComponents)
{
    // Preparation may allocate; OnAttach registration in the commit phase must
    // fit these capacities. The count includes all new Components conservatively.
    m_meshRenderers.reserve(m_meshRenderers.size() + additionalComponents);
    m_spriteRenderers.reserve(m_spriteRenderers.size() + additionalComponents);
    m_uiRenderers.reserve(m_uiRenderers.size() + additionalComponents);
}

void RenderSystem::FlushRegisters()
{
    const auto flush = [](const auto& renderers)
    {
        for (auto* renderer : renderers)
        {
            Actor* actor = renderer ? renderer->GetOwner() : nullptr;
            const bool live = IsActorHierarchyActive(actor);
            if (live && !renderer->IsDestroyed())
            {
                renderer->Flush();
            }
        }
    };

    flush(m_meshRenderers);
    flush(m_spriteRenderers);
    flush(m_uiRenderers);
}

bool RenderSystem::AppendFrame(rendering::FrameRenderData& frame, const rendering::ViewOutput& output,
    const CameraInfo* camera, const RenderViewPolicy& policy, rendering::ViewId sceneViewId,
    rendering::ViewId overlayViewId, std::uint32_t width, std::uint32_t height,
    const DirectionalLight& light)
{
    if (!m_scene || width == 0 || height == 0)
    {
        return false;
    }

    const EngineContext* context = m_scene->GetEngineContext();
    if (!context)
    {
        return false;
    }

    m_records.clear();
    const bool canvasPreview = policy.canvasPreview || policy.canvasViewRoot;
    Matrix4x4 worldToCanvas = Matrix4x4::Identity();
    if (policy.canvasViewRoot)
    {
        worldToCanvas = policy.canvasViewRoot->GetContentWorldMatrix().Inverse();
    }

    std::vector<SubmissionSource> sources;
    const auto gather = [&](const auto& renderers, SourceKind kind)
    {
        for (auto* renderer : renderers)
        {
            Actor* actor = renderer ? renderer->GetOwner() : nullptr;
            const bool liveActor = IsActorHierarchyActive(actor);
            if (!liveActor || renderer->IsDestroyed())
            {
                continue;
            }

            if (!renderer->IsVisible() || !renderer->IsConfigured())
            {
                continue;
            }

            if (!renderer->SynchronizeRenderResources())
            {
                return false;
            }

            const RenderSpace space = renderer->GetRenderSpace();
            if (policy.canvasViewRoot)
            {
                if (!policy.canvasViewRoot->ContainsRenderer(renderer))
                {
                    continue;
                }
            }
            else
            {
                const bool excludeWorld = policy.renderSpaceFilter == RenderSpaceFilter::ScreenOnly && space == RenderSpace::World;
                const bool excludeScreen = policy.renderSpaceFilter == RenderSpaceFilter::WorldOnly && space == RenderSpace::Screen;
                if (excludeWorld || excludeScreen)
                {
                    continue;
                }
            }

            SubmissionSource source;
            source.component = renderer;
            source.kind = kind;
            source.screen = canvasPreview || space == RenderSpace::Screen;
            if (!source.screen && !camera)
            {
                continue;
            }

            if (source.screen)
            {
                source.orderPath = renderer->BuildCanvasSortPath();
            }

            sources.push_back(std::move(source));
        }

        return true;
    };

    if (!gather(m_meshRenderers, SourceKind::Mesh) || !gather(m_spriteRenderers, SourceKind::Sprite) ||
        !gather(m_uiRenderers, SourceKind::UI))
    {
        return false;
    }

    // Sort only submission metadata, not a second World/Screen Object array.
    // The final single owning array already has the required Overlay order.
    std::stable_sort(sources.begin(), sources.end(), [](const SubmissionSource& left, const SubmissionSource& right)
    {
        if (left.screen != right.screen)
        {
            return !left.screen;
        }

        return left.screen && left.orderPath < right.orderPath;
    });

    if (camera && !canvasPreview && policy.renderSpaceFilter != RenderSpaceFilter::ScreenOnly)
    {
        rendering::RenderView sceneView;
        sceneView.id = sceneViewId;
        sceneView.output = output;
        sceneView.view = EngineRendering::ToRenderer(camera->viewMatrix);
        sceneView.projection = EngineRendering::ToRenderer(camera->projMatrix);
        sceneView.cameraPosition = EngineRendering::ToRenderer(camera->position);
        sceneView.exposure = 1.0f; // R8 implementation default; no editing/auto-exposure API.
        frame.views.push_back(sceneView);

        if (light.enabled)
        {
            rendering::DirectionalLight input;
            input.lightDirection[0] = light.direction.x;
            input.lightDirection[1] = light.direction.y;
            input.lightDirection[2] = light.direction.z;
            input.lightColor[0] = light.color.x * light.intensity;
            input.lightColor[1] = light.color.y * light.intensity;
            input.lightColor[2] = light.color.z * light.intensity;
            input.ambient[0] = light.ambient;
            input.ambient[1] = light.ambient;
            input.ambient[2] = light.ambient;
            frame.directionalLight = input;
        }
    }

    rendering::RenderView overlay;
    overlay.id = overlayViewId;
    overlay.kind = rendering::ViewKind::Overlay;
    overlay.output = output;
    if (canvasPreview && camera)
    {
        overlay.view = EngineRendering::ToRenderer(camera->viewMatrix);
        overlay.projection = EngineRendering::ToRenderer(camera->projMatrix);
        overlay.cameraPosition = EngineRendering::ToRenderer(camera->position);
    }
    else
    {
        overlay.projection = rendering::Matrix4x4::CreateOrthographic(static_cast<float>(width),
            static_cast<float>(height), -1, 100);
        if (camera)
        {
            overlay.cameraPosition = EngineRendering::ToRenderer(camera->position);
        }
    }

    // Always provide a background/Screen View, including no Main Camera.
    frame.views.push_back(overlay);

    const auto append = [&](rendering::RenderObject object, const SubmissionSource& source,
                            const ComponentMaterialState& material, bool alphaMask)
    {
        object.targetViews.push_back(source.screen ? overlayViewId : sceneViewId);
        ExtractedObjectRecord record;
        record.objectIndex = frame.objects.size();
        record.actorGuid = source.component->GetOwner()->GetGuid();
        record.material = &material;
        record.alphaMask = alphaMask;
        record.viewId = source.screen ? overlayViewId : sceneViewId;
        frame.objects.push_back(std::move(object));
        m_records.push_back(record);
    };

    CameraInfo emptyCamera;
    const CameraInfo& proxyCamera = camera ? *camera : emptyCamera;
    for (const SubmissionSource& source : sources)
    {
        if (source.kind == SourceKind::Mesh)
        {
            auto* renderer = static_cast<MeshRenderer*>(source.component);
            const auto& proxy = renderer->GetRenderProxy();
            for (const SubmeshRenderTemplate& renderTemplate : renderer->GetRenderTemplates())
            {
                Matrix4x4 world = renderTemplate.localToModel * proxy.common.worldMatrix * worldToCanvas;
                rendering::RenderObject object;
                object.geometry = renderTemplate.meshDesc.meshHandle;
                object.material = renderTemplate.materialDesc.handle;
                object.world = EngineRendering::ToRenderer(world);
                object.tint = EngineRendering::ToRenderer(proxy.common.color);
                object.castShadow = renderer->GetCastShadow() && !source.screen;
                object.drawRange.first = renderTemplate.meshDesc.startIndex;
                object.drawRange.baseVertex = renderTemplate.meshDesc.baseVertex;
                SetWorldBounds(object, renderTemplate.meshDesc.boundsCenter, renderTemplate.meshDesc.boundsRadius,
                    renderTemplate.meshDesc.boundsRevision, world);
                append(std::move(object), source, renderTemplate.materialDesc, false);
            }

            continue;
        }

        if (!context->pRenderAssets)
        {
            continue;
        }

        const auto appendQuad = [&](const CommonRendererProxy& common, const ComponentMaterialState& material,
                                    rendering::BillboardMode billboard, bool castShadow)
        {
            Matrix4x4 world = common.worldMatrix * worldToCanvas;
            rendering::RenderObject object;
            object.geometry = context->pRenderAssets->Quad();
            object.material = material.handle;
            object.world = EngineRendering::ToRenderer(world);
            object.tint = EngineRendering::ToRenderer(common.color);
            object.billboard = billboard;
            object.castShadow = castShadow && !source.screen;
            if (context->pRenderer)
            {
                rendering::GeometryData snapshot;
                const auto queried = context->pRenderer->GetGeometryData(object.geometry, snapshot);
                if (queried.IsSuccess())
                {
                    const rendering::Vector4 local = LocalQuadTransform(material);
                    Vector3 center = EngineRendering::FromRenderer(snapshot.boundsCenter);
                    center.x = center.x * local.x + local.z;
                    center.y = center.y * local.y + local.w;
                    const float localStretch = (std::max)({ std::abs(local.x), std::abs(local.y), 1.0f });
                    SetWorldBounds(object, center, snapshot.boundsRadius * localStretch, snapshot.revision, world);
                }
            }

            append(std::move(object), source, material, true);
        };

        if (source.kind == SourceKind::Sprite)
        {
            auto* renderer = static_cast<SpriteRenderer*>(source.component);
            const auto& proxy = renderer->GetRenderProxy(proxyCamera);
            const auto mode = static_cast<rendering::BillboardMode>(renderer->GetBillboardType());
            appendQuad(proxy.common, renderer->GetRenderTemplate().materialDesc, mode, renderer->GetCastShadow());
        }
        else
        {
            auto* renderer = static_cast<UIRenderer*>(source.component);
            const auto& proxy = renderer->GetRenderProxy();
            for (const UIRenderElement& element : renderer->GetRenderTemplate())
            {
                appendQuad(proxy.common, element.materialDesc, rendering::BillboardMode::None, false);
            }
        }
    }

    return true;
}
