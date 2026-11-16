#include "../../Renderer/shaders/Common.hlsli"

struct SkinVertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    uint4 boneIndices : BLENDINDICES0;
    float4 boneWeights : BLENDWEIGHT0;
};

struct BoneMatrix
{
    row_major float4x4 transform;
};

StructuredBuffer<BoneMatrix> gBoneMatrices : register(t5);

PixelInput main(SkinVertexInput input)
{
    uint boneCount;
    uint elementStride;
    gBoneMatrices.GetDimensions(boneCount, elementStride);
    
    float3 skinnedPosition = 0;
    float3 skinnedNormal = 0;
    float totalWieight = 0;
    
    // Maximum 4 bone can affect a vertex (also limited in Framework).
    [unroll]
    for (uint influence = 0; influence < 4; ++influence)
    {
        float weight = input.boneWeights[influence];
        uint bone = input.boneIndices[influence];
        
        if (weight > 0 && bone < boneCount)
        {
            float4x4 transform = gBoneMatrices[bone].transform;
            skinnedPosition += mul(float4(input.position, 1), transform).xyz * weight;
            skinnedNormal += mul(float4(input.normal, 0), transform).xyz * weight;
            totalWieight += weight;
        }
    }
    
    float3 localPosition = input.position;
    float3 localNormal = input.normal;
    
    if (totalWieight > 0)
    {
        localPosition = skinnedPosition / totalWieight;
        
        if (dot(skinnedNormal, skinnedNormal) > 1e-12f)
        {
            localNormal = normalize(skinnedNormal);
        }
    }
    
    PixelInput output;
    float4 worldPosition = mul(float4(localPosition, 1), world);
    output.position = mul(mul(worldPosition, view), projection);
    output.worldPosition = worldPosition.xyz;
    output.normal = mul(float4(localNormal, 0), normalMatrix).xyz;
    output.uv = input.uv;
    output.color = input.color * tint;
    
    return output;
}
