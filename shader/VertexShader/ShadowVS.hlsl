#include "../FrameConstants.hlsli"
#include "../MeshObjectConstants.hlsli" 

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangent : TANGENT;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 svpos : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;

    float4 worldPos = mul(world, float4(input.position, 1.0f));
    float4 viewPos = mul(view, worldPos);
    float4 projPos = mul(proj, viewPos);

    output.svpos = projPos;
    return output;
}