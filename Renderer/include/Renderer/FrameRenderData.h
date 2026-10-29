// R7 implementation guide: section 4.4.
// CPU-owned frame values and non-owning resource identities are deliberately separate.
// Submit moves the frame values, but Render resolves the latest accepted resource revisions.
// Each View selects original Object indices; no FrozenObject conversion or parallel update is added.

#pragma once

#include "ResourceHandle.h"
#include "WindowOutput.h"
#include "DrawTypes.h"
#include "LightTypes.h"

#include <array>
#include <optional>
#include <variant>
#include <vector>

namespace rendering
{

class Renderer;
// IDs are unique only within a submitted frame.
// An Object may list these IDs explicitly; an empty list selects every View.
using ViewId = std::uint32_t;
using ViewOutput = std::variant<WindowOutputHandle, RenderTargetTextureHandle>;

// Scene performs opaque/transparent selection and clears a private output Depth.
// Overlay preserves submission order, omits Depth and loads color only after an earlier frame pass wrote it.
enum class ViewKind
{
    Scene,
    Overlay
};

// This explicit per-Object mode controls Renderer-side facing during each View.
// The Engine must submit the original World, not an already billboarded proxy World.
enum class BillboardMode
{
    None,
    Spherical,
    Cylindrical
};

// Pixel-space viewport in the selected output.
// It maps projected coordinates to pixels; it does not limit a Scene attachment Clear.
struct Viewport
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
};

// Pixel-space half-open clipping rectangle used by the rasterizer.
// It is intersected with the View/output; every Draw records its effective rectangle.
struct ScissorRect
{
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

// World-space sphere: the submitter includes any vertex deformation.
// Optional caller-supplied world-space culling bound.
// Include shader deformation and billboard rotation in the bound; the Renderer cannot infer them.
// A missing bound means no frustum culling, not an invisible Object.
struct BoundingSphere
{
    Vector3 center;
    float radius = 0;
};

// Explicit no-VB/no-IB path, typically driven by SV_VertexID.
// A nonzero vertexCount is required; the normal Geometry handle and DrawRange must be absent/default.
struct ProceduralGeometry
{
    std::uint32_t vertexCount = 0;
};

// For registered Geometry, first/count select indices when indexed, otherwise vertices.
// count == 0 means the remaining data. baseVertex is added to each selected index and must stay in VB range.
// Procedural draws instead take the complete count from ProceduralGeometry.
struct DrawRange
{
    std::uint32_t first = 0;
    // Zero selects the remaining registered Geometry. Procedural uses vertexCount.
    std::uint32_t count = 0;
    std::int32_t baseVertex = 0;
};

// One immutable submitted value may appear in multiple View index lists.
// Billboard changes only temporary draw constants; the stored World is never overwritten.
struct RenderObject
{
    GeometryHandle geometry;
    MaterialHandle material;
    Matrix4x4 world = Matrix4x4::Identity();
    Vector4 tint{ 1, 1, 1, 1 };
    // Empty means every View. Overlay participants should use explicit membership.
    std::vector<ViewId> targetViews;
    // No persistent Content pointers are stored with Object membership.
    // Each resource handle is resolved against its owner during Render.
    std::optional<BoundingSphere> bounds;
    BillboardMode billboard = BillboardMode::None;
    std::optional<ProceduralGeometry> procedural;
    DrawRange drawRange;
    std::optional<ScissorRect> scissor;
};

// Camera/projection/settings are owned frame values.
// Output identity is stable across Resize, but default extent is queried again during Render.
// Resize does not automatically rewrite a previously submitted projection matrix.
struct RenderView
{
    ViewOutput output;
    Matrix4x4 view = Matrix4x4::Identity();
    Matrix4x4 projection = Matrix4x4::Identity();
    Vector3 cameraPosition;
    DrawSettings drawSettings;
    ViewId id = 0;
    ViewKind kind = ViewKind::Scene;
    // Omitted means the current complete output extent, resolved during Render.
    std::optional<Viewport> viewport;
    // The default matches the output allocation's optimized-clear hint.
    // Other clear colors remain valid, but the DX12 debug layer may report a performance warning.
    std::array<float, 4> clearColor{ 0.05f, 0.10f, 0.25f, 1.0f };
};

// Submit owns these values. Resource contents are selected during Render.
// One calling thread; public API mutation/reentry during Render is prohibited.
// The Renderer accepts this value only after validation and then consumes it in one Render.
// It does not snapshot Geometry/Material/Texture Content revisions at Submit time.
// Updates, Destroy or Resize between Submit and Render therefore affect resource resolution.
struct FrameRenderData
{
    std::vector<RenderObject> objects;
    std::vector<RenderView> views;
    std::optional<DirectionalLight> directionalLight;
    float time = 0;

private:
    friend class Renderer;
    bool hasPendingSubmission = false;
};

}
