// Constant-buffer for sprite object constants
cbuffer SpriteObjectConstants : register(b1)
{
    float4x4 world;     // World matrix
    float4 objColor;    // Overall color
    float4 uvRect;      // UV rectangle information (x: left, y: top, z: right, w: bottom)
    float2 pivot;       // Pivot point for sprite rotation
    float2 flip;        // Sprite flip information (x: horizontal flip, y: vertical flip)
};