#include "Common.hlsli"

cbuffer MaterialParams : register(b5)
{
    float4 UnusedPadding;
    float4 BaseColor;
};

Texture2D BaseTexture : register(t0);
SamplerState LinearWrap : register(s0);

float4 main(PixelInput input) : SV_TARGET
{
    return BaseTexture.Sample(LinearWrap, input.uv) * BaseColor * input.color;
}