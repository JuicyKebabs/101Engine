#include "Common.hlsli"

#include "MaterialParams.hlsli"

Texture2D BaseTexture : register(t0);
SamplerState LinearWrap : register(s0);
Texture2D<float> RendererShadowMap : register(t15);
SamplerComparisonState RendererShadowSampler : register(s2);

// The projection volume is finite; objects outside it are unshadowed.
// The map and matrix belong to this Scene View, including its Billboard pose.
float EvaluateDirectionalShadow(float3 worldPosition)
{
    if (shadowInformation.x < 0.5)
    {
        return 1;
    }

    float4 lightClip = mul(float4(worldPosition, 1), shadowViewProjection);
    float3 projected = lightClip.xyz / lightClip.w;
    float2 textureUV = projected.xy * float2(0.5, -0.5) + 0.5;
    bool outsideTexture = any(textureUV < 0) || any(textureUV > 1);
    bool outsideDepth = projected.z < 0 || projected.z > 1;
    if (outsideTexture || outsideDepth)
    {
        return 1;
    }

    return RendererShadowMap.SampleCmpLevelZero(RendererShadowSampler, textureUV,
        projected.z - shadowInformation.y);
}

float4 main(PixelInput input) : SV_TARGET
{
    float3 n = normalize(input.normal);
    float lambert = saturate(dot(n, normalize(-lightDirection.xyz)));
    float4 base = BaseTexture.Sample(LinearWrap, input.uv) * BaseColor * input.color;
    return float4(base.rgb * (ambient.rgb + lightColor.rgb * lambert * EvaluateDirectionalShadow(input.worldPosition)), base.a);
}