#include "../Constants/BasicShader.hlsli"
Texture2D gTexture : register(t0); //テクスチャオブジェクト
SamplerState gSampler : register(s0); //サンプラーオブジェクト

float4 main(
    VSOutPut input //頂点シェーダーから送られてきたデータ構造体
) : SV_TARGET //レンダーターゲットへ出力
{
    float4 base = gTexture.Sample(gSampler, input.uv);
    
    return base;
}