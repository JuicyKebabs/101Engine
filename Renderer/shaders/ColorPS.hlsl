#include "Common.hlsli"

cbuffer MaterialParams : register(b5)
{
    float4 BaseColor;
};

float4 main(PixelInput input) : SV_TARGET
{
    return BaseColor * input.color;
}