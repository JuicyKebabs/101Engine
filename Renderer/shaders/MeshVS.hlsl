#include "Common.hlsli"
#include "MaterialParams.hlsli"

PixelInput main(VertexInput input)
{
    // Apply pivot and flip before World so billboards keep the same anchor.}
    return TransformMeshVertex(input, LocalQuadTransform);
}