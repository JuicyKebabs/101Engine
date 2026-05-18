#include "../Constants/BasicShader.hlsli"

VSOutPut main(
    uint vertexID : SV_VertexID //頂点ID
)
{
    float2 pos[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f),
    };
    
    VSOutPut output = (VSOutPut) 0; //アウトプット構造体を０クリア
    output.svpos = float4(pos[vertexID], 0.0f, 1.0f);
    
    output.uv = pos[vertexID] * 0.5f + 0.5f; // UV座標を設定
    output.uv.y = 1.0f - output.uv.y; // UV座標のY軸を反転
    
    return output;
}