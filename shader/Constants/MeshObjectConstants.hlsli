// Constant buffer for mesh object constants
cbuffer MeshObjectconstants : register(b1)
{
    float4x4 world;
    float4x4 worldInvTranspose;
    float4x4 lightViewProj;
    float4 objColor;
};