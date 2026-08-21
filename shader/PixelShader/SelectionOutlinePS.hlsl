#include "../Constants/BasicShader.hlsli"

Texture2D gSelectionMask : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutPut input) : SV_TARGET
{    
    uint width;
    uint height;

    gSelectionMask.GetDimensions(width, height);

    const int2 textureMax = int2(int(width) - 1, int(height) - 1);

    const int2 centerPosition = clamp(int2(input.svpos.xy), int2(0, 0), textureMax);

    const float center = gSelectionMask.Load(int3(centerPosition, 0)).r;
    
    float expanded = 0.0f;
    
    // Search neighboring pixels to expand the selected silhouette
	[unroll]
    for (int y = -2; y <= 2; y++)
    {
		[unroll]
        for (int x = -2; x <= 2; x++)
        {
            const int2 samplePosition =
			    clamp(
					centerPosition + int2(x, y),
					int2(0, 0),
					textureMax
				);
            
            const float sampleValue =
				gSelectionMask.Load(
					int3(samplePosition, 0)
				).r;
            
            expanded = max(expanded, sampleValue);
        }
    }
    
    // Remove the original silhouette, leaving only its outer edge
    const float outline = saturate(expanded - center);

    clip(outline - 0.01f);

    const float3 outlineColor = float3(1.0f, 0.55f, 0.1f);

    return float4(outlineColor, outline);
}