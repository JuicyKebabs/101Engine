#include "EditorCommon.hlsli"

//-------------------------------------------------------------------------
// AlphaMask.hlsl
// Generate mask for a alpha object, used for rendering outlines in editor.
//-------------------------------------------------------------------------

Texture2D BaseTexture : register(t0);
SamplerState LinearWrap : register(s0);
float4 main(Input input) : SV_TARGET

{
    float alpha = BaseTexture.Sample(LinearWrap, input.uv * UVTransform.xy + UVTransform.zw).a;
   
    // Clip transparent pixels to avoid outline artifacts.
    // The alpha values below 0.05 are considered transparent.
    clip(alpha * BaseColor.a * input.color.a - 0.05f);
    
    return float4(1, 1, 1, 1);
}