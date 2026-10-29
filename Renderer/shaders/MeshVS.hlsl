#include "Common.hlsli"
#include "MaterialParams.hlsli"

PixelInput main(VertexInput input)
{
    // Keep pivot/flip before the Actor World. Renderer derives Billboard from
    // that unshifted World, so Camera facing and Shadow use the same anchor.
    float3 localPosition = input.position;
    localPosition.xy = localPosition.xy * LocalQuadTransform.xy + LocalQuadTransform.zw;

    PixelInput output;
    output.position = mul(mul(mul(float4(localPosition, 1), world), view), projection);
    output.worldPosition = mul(float4(localPosition, 1), world).xyz;
    output.normal = mul(float4(input.normal, 0), normalMatrix).xyz;
    output.uv = input.uv;
    output.color = input.color * tint;
    
    return output;
}