#include "../../Renderer/shaders/Common.hlsli"

cbuffer PickingParameters : register(b5)
{
    float4 UVTransform;
    float4 LocalQuadTransform;
    uint SelectionId;
};

Texture2D BaseTexture : register(t0);
SamplerState LinearWrap : register(s0);

PixelInput PickingVS(VertexInput input)
{
    return TransformMeshVertex(input, LocalQuadTransform);
}

uint PickingSolidPS() : SV_Target0
{
    return SelectionId;
}

uint PickingTexturePS(PixelInput input) : SV_Target0
{
    const float2 uv = ApplyMaterialUV(input.uv, UVTransform);

    if (BaseTexture.Sample(LinearWrap, uv).a == 0.0)
    {
        discard;
    }

    return SelectionId;
}