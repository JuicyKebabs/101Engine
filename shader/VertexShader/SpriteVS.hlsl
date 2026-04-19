#include "../BasicShader.hlsli"
#include "../FrameConstants.hlsli"
#include "../SpriteObjectConstants.hlsli"

VSOutPut main(
    uint vertexID : SV_VertexID
)
{
    static const float2 quadVertices[6] =
    {
        float2(-0.5, +0.5),
        float2(-0.5, -0.5),
        float2(+0.5, +0.5),
        float2(+0.5, +0.5),
        float2(-0.5, -0.5),
        float2(+0.5, -0.5)
    };
    
    static const float2 quadUVs[6] =
    {
        float2(0.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 0.0),
        float2(1.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 1.0)
    };
    
    float2 localPos = quadVertices[vertexID];
    localPos -= pivot - 0.5;
    localPos *= flip;
    float2 uv01 = quadUVs[vertexID];
    
    VSOutPut output = (VSOutPut) 0;
    float4 worldPos = mul(world, float4(localPos, 0.0f, 1.0f));
    float4 viewPos = mul(view, worldPos);
    output.svpos = mul(proj, viewPos);
    
    output.uv = lerp(uvRect.xy, uvRect.zw, uv01);
    output.color = objColor;
    return output;
}
