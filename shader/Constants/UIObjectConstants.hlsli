// Constant-buffer for UI object constants
cbuffer UiObjectConstants : register(b1)
{
    float4x4 world;     // World matrix
    float4 objColor;    // Overall color
    float4 uvRect;      // UV rectangle information (x: left, y: top, z: right, w: bottom)
    float2 flip;        // UI element flip information (x: horizontal flip, y: vertical flip)
};