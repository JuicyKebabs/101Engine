#include "../Constants/BasicShader.hlsli"
#include "../Constants/FrameConstants.hlsli"
#include "../Constants/MeshObjectConstants.hlsli"
//頂点シェーダー入力データ構造体
struct VSInput
{
    float3 position : POSITION; //頂点座標
    float3 normal : NORMAL; //法線ベクトル
    float2 uv : TEXCOORD0; //テクスチャ座標
    float3 tangent : TANGENT; //接空間
    float4 color : COLOR; //頂点カラー
};

//頂点シェーダーの関数
VSOutPut main(
    VSInput input //頂点シェーダー入力データ構造体
)
{
    VSOutPut output = (VSOutPut) 0; //アウトプット構造体を０クリア
 
    //ワールド行列、ビュー行列、プロジェクション行列を乗算して座標変換する
    float4 localPos = float4(input.position, 1.0f); // 頂点座標
    float4 worldPos = mul(world, localPos); // ワールド座標に変換
    float4 viewPos = mul(view, worldPos); // ビュー座標に変換
    float4 projPos = mul(proj, viewPos); // 投影変換
    
    //出力データの設定
    output.svpos = projPos; //変換後の頂点座標を設定
    output.color = input.color * objColor; //頂点カラーを設定
    output.uv = input.uv; //テクスチャ座標を設定
    output.normal = normalize(mul((float3x3) worldInvTranspose, input.normal)); //法線の設定
    output.worldPos = mul(world, float4(input.position, 1.0f)).xyz; //ワールド座標を設定

    return output;
}