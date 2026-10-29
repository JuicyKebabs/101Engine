#include "Common.hlsli"

#include "MaterialParams.hlsli"

Texture2D ExtraTexture : register(t3);
SamplerState LinearClamp : register(s1);

float4 main(PixelInput input) : SV_TARGET
{
    return ExtraTexture.Sample(LinearClamp, input.uv) * BaseColor * input.color;
}