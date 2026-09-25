#include "../Constants/BasicShader.hlsli"
#include "../Constants/FrameConstants.hlsli"
#include "../Constants/WaveObjectConstants.hlsli"
#include "../Constants/LightConstants.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float PI = 3.14159265359f;

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

float2 DirectionToSkyUV(float3 direction)
{
    float3 d = normalize(direction);

    // XZ平面上の角度を計算
    float theta = 0.0;
    if (dot(d.xz, d.xz) > 1.0e-12)
    {
        theta = atan2(d.z, d.x);
    }

    // 角度をUV座標に変換
    float u = frac(theta / (2.0 * PI) + 1.0);
    float v = 0.5 - asin(clamp(d.y, -1.0, 1.0)) / PI;

    return float2(u, v);
}

float4 main(VSOutPut input) : SV_TARGET
{
    // 反射無しは頂点色をそのまま返す
    if (!isReflective)
    {
        return float4(input.color.rgb, 1.0f);
    }
    
    // 水面の法線と視線方向から反射率を計算
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldPos);
    
    float NdotV = saturate(dot(N, V));
    float F0 = 0.02f;
    float F = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);
    
    // 反射方向
    float3 R = reflect(-V, N);
    
    // 反射方向からスカイドームのUV座標を計算する
    float2 skyUV = DirectionToSkyUV(R);
    
    
    // スカイドームテクスチャの色をサンプリングする（反射色に使用）
    float3 skyColor = gTexture.Sample(gSampler, skyUV, 0.0).rgb;
    
    // 水面の色と反射色を合成
    float3 waterColor = input.color.rgb;
    float3 water = lerp(waterColor, skyColor, F);
    
    // 以下、ライティング計算
    float3 diffuse = lightColor_Ambient.rgb;
    float3 ambient = lightColor_Ambient.a;
    
    float3 L = normalize(-lightDir_Intensity.xyz);
    
    float3 lambert = CreateLambert(N, L, diffuse);
    float specular = CreateSpecular(N, L, V, 2.0f);
    
    float3 lightEffect = (lambert.xyz + specular) + ambient;
    
    // ライティングの影響を水面の色に適用
    water *= lightEffect;

    return float4(water, 1.0);
}


