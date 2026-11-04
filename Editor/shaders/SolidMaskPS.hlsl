//-------------------------------------------------------------------------
// SolidMaskPS.hlsl
// Generate mask for a solid object, used for rendering outlines in editor.
//-------------------------------------------------------------------------

#include "EditorCommon.hlsli"

float4 main(Input input) : SV_TARGET
{
    // Just draw the white mask of a solid object.
    // Other pixels are clear value.
    return float4(1, 1, 1, 1);
}