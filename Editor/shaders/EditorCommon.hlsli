struct Input
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float3 worldPosition : TEXCOORD1;
};

cbuffer MaterialParams : register(b5)
{
    float4 BaseColor;
    float4 UVTransform;
    float4 LocalQuadTransform;
};