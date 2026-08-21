#include "../Constants/BasicShader.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutPut input) : SV_TARGET
{
    // Clip the pixel if its alpha value is below a certain threshold (0.1 in this case)
    const float alpha = gTexture.Sample(gSampler, input.uv).a;
    clip(alpha - 0.1f);

    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}