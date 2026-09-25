#include "../Constants/BasicShader.hlsli"
#include "../Constants/FrameConstants.hlsli"
#include "../Constants/WaveObjectConstants.hlsli"

VSOutPut main(uint vertexID : SV_VertexID)
{
    // 6頂点分の座標定義（セル1つ分）
    static const float2 cellVertices[6] =
    {
        float2(0.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 0.0),
        float2(1.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 1.0)
    };
    
    // 現在の頂点のセルインデックスとセル内の頂点インデックスを計算
    uint cellIndex = vertexID / 6;
    uint vertexInCell = vertexID % 6;
    
    // 分割したQuadのX方向とY方向のインデックスを計算
    uint cellX = cellIndex % subdivisionX;
    uint cellY = cellIndex / subdivisionX;
    
    // セル内の頂点座標を、Quad上のUV座標に変換
    float2 cellVertex = cellVertices[vertexInCell];
    float2 gridPos = float2(cellX, cellY) + cellVertex;
    float2 subdivision = float2(subdivisionX, subdivisionY);
    float2 uv01 = gridPos / subdivision;
    
    // Quad中心を原点とした座標系に変換
    float2 basePos = float2(uv01.x - 0.5, 0.5 - uv01.y);
    
    // Sine波を用いて位相を計算し、頂点の高さを決定
    float phase = dot(basePos, waveDirection) * waveFrequency + time;
    float height = waveAmplitude * sin(phase);
    float3 localPos = float3(basePos, height);
    
    // 波の傾きを計算して法線ベクトルを求める
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
