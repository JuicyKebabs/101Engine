#include "Common.hlsli"

PixelInput main(VertexInput input)
{
    PixelInput output;
    output.position = mul(mul(mul(float4(input.position, 1), world), view), projection);
    output.normal = mul(float4(input.normal, 0), normalMatrix).xyz;
    output.uv = input.uv;
    output.color = input.color * tint;
    
    return output;
}