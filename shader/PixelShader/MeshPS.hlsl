#include "../Constants/BasicShader.hlsli"
#include "../Constants/FrameConstants.hlsli"
#include "../Constants/MeshObjectConstants.hlsli"
#include "../Constants/LightConstants.hlsli"

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

float3 CreateLambert(float3 normal, float3 lightDir, float3 diffuse)
{
    float dotValue = saturate(dot(normal, lightDir));
    dotValue = pow(dotValue, 2.0f);
    return diffuse * dotValue;
}

float3 CreateSpecular(float3 normal, float3 lightDir, float3 viewDir, float materialSpecular)
{
    float3 reflection = reflect(-lightDir, normal);
    float dotValue = saturate(dot(reflection, viewDir));
    dotValue = pow(dotValue, materialSpecular);
    return dotValue;
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
    float3 diffuse = lightColor_Ambient.rgb;
    float3 ambient = lightColor_Ambient.a;
        
    float3 lambert = CreateLambert(input.normal, -lightDir_Intensity.xyz, diffuse);
    float specular = CreateSpecular(input.normal, -lightDir_Intensity.xyz, normalize(cameraPos - input.worldPos), 2.0f);
    float shadow = CalculateShadow(float4(input.worldPos, 1.0f));
    
    float3 lightEffect = (lambert.xyz + specular) * shadow + ambient;
    base.xyz *= lightEffect;
    
#endif
    return base;
}

