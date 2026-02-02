//==============================================================
//頂点シェーダー
//各頂点ごとに呼ばれ頂点の座標を引数として受け取る。
//ラスタライザー → ピクセルシェーダーの順に送られる
//==============================================================
#include "BasicShader.hlsli"
//定数バッファ０
cbuffer PerObject : register(b0)
{
    float4x4 world; //ワールド行列
    float4x4 worldInvTranspose; //ワールド行列の逆転置行列
    float4x4 view; //ビュー行列
    float4x4 proj; //プロジェクション行列
    float4 objColor; //全体の色
    float4 uvRect; //uv矩形情報(x:左, y:上, z:右, w:下)
    
    float4 lightDir_Intensity; //ライトの方向(x,y,z)、強度(w)
    float4 lightColor_Ambient; //ライトの色(x,y,z)、環境光強度(w)
}

//ビルボード用定数バッファ
cbuffer BillboardObject : register(b1)
{
    //カメラデータ
    float4x4 viewProj; //ビュー×プロジェクション行列
    float3 right; //カメラの右方向ベクトル
    float _pad0; //パディング
    float3 up; //カメラの上方向ベクトル
    float _pad1; //パディング

    //ビルボードデータ
    float3 center; //ビルボードの中心座標
    float _pad2; //パディング
    float2 size; //ビルボードのサイズ
    float2 _padSize; //パディング
    float4 colorBil; //ビルボードの色
    float4 uvRectBil; //uv矩形情報(x:左, y:上, z:右, w:下)
};

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
VSOutPut BasicVS(
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
    output.color = input.color; //頂点カラーを設定
    output.uv = uvRect.xy + input.uv * uvRect.zw; //uv座標を設定
    output.normal = normalize(mul((float3x3) worldInvTranspose, input.normal)); //法線の設定

    return output;
}

//ビルボード頂点シェーダーの関数
VSOutPut BillboardVS(
    VSInput input //頂点シェーダー入力データ構造体
)
{
    VSOutPut output = (VSOutPut) 0; //アウトプット構造体
 
    //ビルボードの四隅の頂点位置を計算
    float3 worldPos =
          center
        + (right * (input.position.x * size.x))
        + (up * (input.position.y * size.y));

    //座標変換
    output.svpos = mul(float4(worldPos, 1.0f), viewProj);
    
    //uv座標の計算
    float2 uv = input.uv;
    output.uv = uvRectBil.xy + uv * uvRectBil.zw;
    
    //頂点カラーの設定
    output.color = input.color;
    
    return output;
}