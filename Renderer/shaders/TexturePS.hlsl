#include "Common.hlsli"

#include "MaterialParams.hlsli"

Texture2D BaseTexture : register(t0);
SamplerState LinearWrap : register(s0);

float4 main(PixelInput input) : SV_TARGET
{
    return BaseTexture.Sample(LinearWrap, ApplyMaterialUV(input.uv, UVTransform)) * BaseColor * input.color;
}