//-----------------------------------------------------------------------------------------------
// Common shader include file
// Do not change the register numbers of the constant buffers, as they are used in the C++ code.
// b5 is reserved for material parameter that each additional shader can define for its own use.
//-----------------------------------------------------------------------------------------------

cbuffer RendererFrame : register(b0)
{
    float time;
};

cbuffer RendererView : register(b1)
{
    row_major float4x4 view;
    row_major float4x4 projection;
    float4 cameraPosition;
};

cbuffer RendererLight : register(b2)
{
    float4 lightDirection;
    float4 lightColor;
    float4 ambient;
};

cbuffer RendererPass : register(b3)
{
    float4 passColor;
};

cbuffer RendererObject : register(b4)
{
    row_major float4x4 world;
    row_major float4x4 normalMatrix;
    float4 tint;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 tangent : TANGENT;
    float4 color : COLOR0;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};