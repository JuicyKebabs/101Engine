#include "Common.hlsli"

cbuffer MaterialParams : register(b5)
{
    float4 BaseColor;
};

Texture2D BaseTexture : register(t0);
SamplerState LinearWrap : register(s0);

float4 main(PixelInput input) : SV_TARGET
{
    float3 n = normalize(input.normal);
    float lambert = saturate(dot(n, normalize(-lightDirection.xyz)));
    float4 base = BaseTexture.Sample(LinearWrap, input.uv) * BaseColor * input.color;
    return float4(base.rgb * (ambient.rgb + lightColor.rgb * lambert), base.a);
}