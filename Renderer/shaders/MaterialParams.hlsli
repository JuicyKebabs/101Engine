// The standard Engine shaders share exactly one b5 layout across VS and PS.
// Mesh uses identity UV/Quad values. Sprite/UIImage update only their own Material.
cbuffer MaterialParams : register(b5)
{
    float4 BaseColor;
    float4 UVTransform;        // xy: UV scale, zw: UV offset
    float4 LocalQuadTransform; // xy: position reflection, zw: local position offset
};
