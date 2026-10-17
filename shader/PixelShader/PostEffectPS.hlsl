#include "../Constants/BasicShader.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

static const bool BloomEnabled = true;
static const float BloomThreshold = 0.7f;
static const float BloomIntensity = 1.0f;

// Symmetric binomial weights approximate a Gaussian; the full kernel sums to 1.
static const float BlurWeights[5] =
{
    70.0f / 256.0f,
    56.0f / 256.0f,
    28.0f / 256.0f,
     8.0f / 256.0f,
     1.0f / 256.0f
};

float3 ExtractBright(float3 color)
{
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float contribution = max(luminance - BloomThreshold, 0.0f)
        / max(luminance, 0.00001f);
    return color * contribution;
}

float4 main(VSOutPut input) : SV_TARGET
{
    float4 base = gTexture.Sample(gSampler, input.uv);
    if (!BloomEnabled)
    {
        return base;
    }

    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    float2 minUV = texelSize * 0.5f;
    float2 maxUV = 1.0f - minUV;
    float3 bloom = 0.0f;

    // ponytail: 81 filter samples per pixel; use half-resolution separable
    // passes if GPU cost or a wider glow becomes necessary.
    [unroll]
    for (int y = -4; y <= 4; ++y)
    {
        [unroll]
        for (int x = -4; x <= 4; ++x)
        {
            float2 uv = input.uv + float2(x, y) * texelSize;
            // Clamp to texel centers to prevent the existing sampler wrapping.
            uv = clamp(uv, minUV, maxUV);
            float3 color = gTexture.SampleLevel(gSampler, uv, 0).rgb;
            float weight = BlurWeights[abs(x)] * BlurWeights[abs(y)];
            bloom += ExtractBright(color) * weight;
        }
    }

    return float4(base.rgb + bloom * BloomIntensity, base.a);
}
