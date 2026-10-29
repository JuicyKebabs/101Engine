#pragma once

#include "Renderer/FrameRenderData.h"
#include "Engine/Core/GUID/Guid.h"
#include "Engine/Graphics/LightTypes.h"
#include "Engine/Graphics/RenderTemplateFactory.h"

class Actor;
class Canvas;
class SceneBase;
class MeshRenderer;
class SpriteRenderer;
class UIRenderer;
class RendererComponent;
struct CameraInfo;

enum class RenderSpaceFilter
{
    All,
    WorldOnly,
    ScreenOnly
};

struct RenderViewPolicy
{
    RenderSpaceFilter renderSpaceFilter = RenderSpaceFilter::All;
    const Canvas* canvasViewRoot = nullptr;
    // Editor navigation chooses the projection for a Canvas preview. No Canvas
    // meaning crosses the Renderer API; it receives only View/Object values.
    bool canvasPreview = false;
};

// Temporary Engine-side metadata for Editor extraction. It points into live
// Components only while the host builds this Frame, and is never submitted.
struct ExtractedObjectRecord
{
    std::size_t objectIndex = 0;
    Guid actorGuid;
    const ComponentMaterialState* material = nullptr;
    bool alphaMask = false;
    rendering::ViewId viewId = 0;
};

class RenderSystem
{
public:
    explicit RenderSystem(SceneBase* scene = nullptr) : m_scene(scene) {}
    void Register(MeshRenderer* renderer);
    void Register(SpriteRenderer* renderer);
    void Register(UIRenderer* renderer);
    void Unregister(MeshRenderer* renderer);
    void Unregister(SpriteRenderer* renderer);
    void Unregister(UIRenderer* renderer);
    void PrepareRegistrationCapacity(std::size_t additionalComponents);
    void FlushRegisters();

    // One extraction walk updates Component snapshots and appends one Object
    // array. Camera/Light visibility and transparent sorting remain Renderer work.
    bool AppendFrame(rendering::FrameRenderData& frame, const rendering::ViewOutput& output,
        const CameraInfo* camera, const RenderViewPolicy& policy, rendering::ViewId sceneViewId,
        rendering::ViewId overlayViewId, std::uint32_t width, std::uint32_t height,
        const DirectionalLight& light);
    const std::vector<ExtractedObjectRecord>& GetExtractedObjects() const noexcept { return m_records; }

private:
    SceneBase* m_scene = nullptr;
    std::vector<MeshRenderer*> m_meshRenderers;
    std::vector<SpriteRenderer*> m_spriteRenderers;
    std::vector<UIRenderer*> m_uiRenderers;
    std::vector<ExtractedObjectRecord> m_records;
};
