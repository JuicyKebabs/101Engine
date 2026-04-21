#include "../BasicShader.hlsli"
#include "../FrameConstants.hlsli"
#include "../MeshObjectConstants.hlsli"
#include "../LightConstants.hlsli"

Texture2D gTexture : register(t0); // Texture object
Texture2D gShadowMap : register(t1); // Shadow map texture object
SamplerState gSampler : register(s0); // Texture sampler state
SamplerComparisonState gShadowSampler : register(s1); // Shadow map sampler state

float CalculateShadow(float4 worldPos)
{
    float4 lightSpacePos = mul(lightViewProj, worldPos);
    lightSpacePos.xyz /= lightSpacePos.w;

    float2 shadowUV;
    shadowUV.x = lightSpacePos.x * 0.5f + 0.5f;
    shadowUV.y = -lightSpacePos.y * 0.5f + 0.5f;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f || shadowUV.y < 0.0f || shadowUV.y > 1.0f)
        return 1.0f;

    float currentDepth = lightSpacePos.z;

    if (currentDepth < 0.0f || currentDepth > 1.0f)
        return 1.0f;

    return gShadowMap.SampleCmpLevelZero(gShadowSampler, shadowUV, currentDepth);
}

float4 main(VSOutPut input) : SV_TARGET
{
    float4 base = gTexture.Sample(gSampler, input.uv) * input.color;
    
#ifdef USE_MASK
     clip(base.a - 0.1f);
#endif
    
#ifdef MULTIPLY_ALPHA_CONTROL
    base.rgb *= base.a;
#endif

#ifdef USE_LIGHTING
    float3 normal = normalize(input.normal);
    float3 length = normalize(-lightDir_Intensity.xyz);
    float dotValue = saturate(dot(normal, length));
    
    float intensity = lightDir_Intensity.w;
    float3 color = lightColor_Ambient.rgb;
    float ambient = lightColor_Ambient.a;
    
    float4 worldPos = float4(input.worldPos, 1.0f);
    float shadow = CalculateShadow(worldPos);
    
    float3 lit = color * (ambient + dotValue * intensity) * shadow;
    base = float4(base.rgb * lit, base.a);
    
#endif
    return base;
}

