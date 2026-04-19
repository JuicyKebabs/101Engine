#include "../BasicShader.hlsli"
#include "../FrameConstants.hlsli"
#include "../SpriteObjectConstants.hlsli"

Texture2D gTexture : register(t0); //テクスチャオブジェクト
SamplerState gSampler : register(s0); //サンプラーオブジェクト

float4 main(
    VSOutPut input //頂点シェーダーから送られてきたデータ構造体
) : SV_TARGET //レンダーターゲットへ出力
{
    float4 base = gTexture.Sample(gSampler, input.uv) * input.color;
    
#ifdef USE_MASK
     clip(base.a - 0.1f);
#endif
    
#ifdef MULTIPLY_ALPHA_CONTROL
    base.rgb *= base.a;
#endif
    
    return base;
}
