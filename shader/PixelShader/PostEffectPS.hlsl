#include "../Constants/BasicShader.hlsli"
Texture2D gTexture : register(t0); //テクスチャオブジェクト
SamplerState gSampler : register(s0); //サンプラーオブジェクト

cbuffer PostEffectConstants : register(b3)
{
    float gTimeSeconds;
};

float4 ChromaticAberration(float2 uv, float strength, float offsetStrength = 1.0f)
{
    float2 center = float2(0.5, 0.5);
    float2 centeredUV = uv - center;
    float2 offset = centeredUV * strength * offsetStrength;
    
    uint width, height;
    gTexture.GetDimensions(width, height);
    
    // Half texel is the center of the top-left pixel in UV space
    // (Zero is the top-left corner of the texture, and 1 is the bottom-right corner)
    float2 halfTexel = 0.5 / float2(width, height);
    
    // Get the each channel's value from the coordinates after offsetting them
    float2 redUV = clamp(uv + offset, halfTexel, 1.0f - halfTexel);
    float2 greenUV = clamp(uv, halfTexel, 1.0f - halfTexel);
    float2 blueUV = clamp(uv - offset, halfTexel, 1.0f - halfTexel);

    float4 base;
    base.r = gTexture.Sample(gSampler, redUV).r;
    base.g = gTexture.Sample(gSampler, greenUV).g;
    base.b = gTexture.Sample(gSampler, blueUV).b;
    base.a = gTexture.Sample(gSampler, greenUV).a;
    return base;
}

float3 DownSaturation(float4 baseColor, float3 colorTint, float saturation)
{
    float luminance = dot(baseColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float3 grayScale = float3(luminance, luminance, luminance);
    float3 saturatedColor = lerp(grayScale, baseColor.rgb, saturation);
    float3 tintedColor = saturatedColor * colorTint;
    return tintedColor;
}

float Vignette(float2 uv)
{
    float2 center = float2(0.5, 0.5);
    float2 centeredUV = uv - center;
    float dist = length(centeredUV);
    float vignette = smoothstep(0.2, 0.6, dist);
    return vignette;
}

uint HashUint(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float RondomNoise(uint2 cell, uint noiseFrame)
{
    uint seed = HashUint(cell.x);
    seed = HashUint(seed ^ cell.y);
    seed = HashUint(seed ^ noiseFrame);
    return float(seed >> 8) / 16777216.0f;
}

float3 ApplyGrain(float3 color, float2 pixelPosition, float noiseStrength, float timeSeconds)
{
    float grainSize = 2.0f;
    uint2 cell = (uint2) floor(pixelPosition / grainSize);
    
    // Swith the noise frame every 1/24 seconds (24 FPS)
    uint noiseFrame = (uint)floor(timeSeconds * 24.0f);
        
    float noise = RondomNoise(cell, noiseFrame);
    float signedNoise = noise * 2.0f - 1.0f; // -1.0 to 1.0
    return color + signedNoise * noiseStrength;
}

float2 DistortUV(float2 uv, float timeSeconds, float strength, float frequency)
{
    float2 centeredUV = uv - float2(0.5f, 0.5f);

    float wave = sin(timeSeconds * frequency * 6.2831853f);
    
    float radiusSquared = dot(centeredUV, centeredUV);
    float edgeWIdth = saturate(radiusSquared * 2.0f);
    
    float2 offset = centeredUV * edgeWIdth * wave * strength;
    
    return uv + offset;

}

float4 main(
    VSOutPut input //頂点シェーダーから送られてきたデータ構造体
) : SV_TARGET //レンダーターゲットへ出力dw
{
    float2 distortedUV = DistortUV(input.uv, gTimeSeconds, 0.2f, 0.8f);
    
    float4 base = ChromaticAberration(distortedUV, 0.02f, 0.8f);
    
    base.rgb = DownSaturation(base, float3(0.65f, 0.85f, 1.0f), 0.9f);
    
    base.rgb = ApplyGrain(base.rgb, input.svpos.xy, 0.05f, gTimeSeconds);
    
    float brightnessByVignette = 1.0f - Vignette(input.uv);
    base.rgb *= brightnessByVignette;
    
    return base;
}