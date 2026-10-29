#include "DisplayTransform.hlsli"

cbuffer MaterialParams : register(b5)
{
    float Exposure;
};

Texture2D HDRTexture : register(t0);
SamplerState LinearClamp : register(s1);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 source = HDRTexture.Sample(LinearClamp, uv);
    float3 exposed = max(source.rgb, 0) * Exposure;
    float3 displayLinear = ReinhardPerChannel(exposed);
    // sRGB RTV storage performs the final encoding; Overlay blending is linear.
    return float4(displayLinear, source.a);
}
