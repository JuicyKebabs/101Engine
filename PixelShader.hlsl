//================================================================================================
//ピクセルシェーダー
//頂点シェーダーから送られてきた座標情報を受け取り、レンダーターゲットへ書き込むための色を返す。
//================================================================================================
#include "BasicShader.hlsli"
//定数バッファ０
cbuffer PerObject : register(b0)
{
    float4x4 world; //ワールド行列
    float4x4 worldInvTranspose; //ワールド行列の逆転置行列
    float4x4 view; //ビュー行列
    float4x4 proj; //プロジェクション行列
    float4 objColor; //全体の色
    float4 uvRect; //uv矩形情報(x:左, y:上, z:右, w:下)
    
    float4 lightDir_Intensity; //ライトの方向(x,y,z)、強度(w)
    float4 lightColor_Ambient; //ライトの色(x,y,z)、環境光強度(w)
}

Texture2D gTexture : register(t0); //テクスチャオブジェクト
SamplerState gSampler : register(s0); //サンプラーオブジェクト

float4 BasicPS(
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