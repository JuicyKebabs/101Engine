#include "../Constants/BasicShader.hlsli"
#include "../Constants/FrameConstants.hlsli"
#include "../Constants/WaveObjectConstants.hlsli"

VSOutPut main(uint vertexID : SV_VertexID)
{
    static const float2 cellVertices[6] =
    {
        float2(0.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 0.0),
        float2(1.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 1.0)
    };
    
    uint cellIndex = vertexID / 6;
    uint vertexInCell = vertexID % 6;
    
    uint cellX = cellIndex % subdivisionX;
    uint cellY = cellIndex / subdivisionX;
    
    float2 cellVertex = cellVertices[vertexInCell];
    
    float2 gridPos = float2(cellX, cellY) + cellVertex;
    
    float2 subdivision = float2(subdivisionX, subdivisionY);
    float2 uv01 = gridPos / subdivision;
    
    float2 basePos = float2(uv01.x - 0.5, 0.5 - uv01.y);
    
    float phase = dot(basePos, waveDirection) * waveFrequency + time;
    float height = waveAmplitude * sin(phase);
    
    float3 localPos = float3(basePos, height);
    
    float2 gradient = waveAmplitude * waveFrequency * cos(phase) * waveDirection;
    float3 localNormal = normalize(float3(gradient, -1.0));
    
    float4 worldPos = mul(world, float4(localPos, 1.0));
    float4 viewPos = mul(view, worldPos);
    
    VSOutPut output = (VSOutPut) 0;
    output.svpos = mul(proj, viewPos);
    output.color = color;
    output.uv = uv01;
    output.normal = normalize(mul((float3x3) worldInvTranspose, localNormal));
    output.worldPos = worldPos.xyz;

    return output;
}
