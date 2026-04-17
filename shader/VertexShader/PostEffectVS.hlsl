#include "../BasicShader.hlsli"
#include "../MeshObjectConstants.hlsli"
//頂点シェーダー入力データ構造体
struct VSInput
{
    float3 position : POSITION; //頂点座標
    float3 normal : NORMAL; //法線ベクトル
    float2 uv : TEXCOORD0; //テクスチャ座標
    float3 tangent : TANGENT; //接空間
    float4 color : COLOR; //頂点カラー
};

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