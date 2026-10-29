#include "Common.hlsli"

#include "MaterialParams.hlsli"

float4 main(PixelInput input) : SV_TARGET
{
    return BaseColor * input.color;
}