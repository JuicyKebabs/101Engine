#include "../BasicShader.hlsli"
#include "../FrameConstants.hlsli"
#include "../MeshObjectConstants.hlsli"
#include "../LightConstants.hlsli"
Texture2D gTexture : register(t0); //テクスチャオブジェクト
SamplerState gSampler : register(s0); //サンプラーオブジェクト

float4 main(
    VSOutPut input //頂点シェーダーから送られてきたデータ構造体
) : SV_TARGET //レンダーターゲットへ出力
{
    float4 base = gTexture.Sample(gSampler, input.uv) * input.color * objColor;
    
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
    
    float3 lit = color * (ambient + dotValue * intensity);
    base = float4(base.rgb * lit, base.a);
#endif
    
    return base;
}

