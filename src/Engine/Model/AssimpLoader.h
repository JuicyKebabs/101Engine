#pragma once
#include "d3d12.h"
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "Engine/Graphics/VertexTypes.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Core/Math/Math.h"

struct Vertex;	// 頂点データ構造体

struct aiMesh;		//Assimpのメッシュ構造体
struct aiMaterial;	//Assimpのマテリアル構造体

//インポート設定構造体
struct ImportSettings
{
	const wchar_t* fileName{};	//ファイルパス
	std::vector<Mesh>& meshs;	//メッシュデータ配列への参照
	bool inverseU = false;		//UVを反転するかどうか
	bool inverseV = false;		//UVを反転するかどうか
};

//Assimpを使ったモデルローダークラス
class AssimpLoader
{
public:
	static bool Load(ImportSettings setting);	//モデル読み込み関数

private:
	static void LoadMesh(	//メッシュ読み込み関数
		Mesh& mesh,	//メッシュデータ構造体への参照
		const aiMesh* src,		//Assimpのメッシュ構造体へのポインタ
		bool inverseU,			//Uを反転するかどうか
		bool inverseV			//Vを反転するかどうか
	);

	static void LoadTexture(	//テクスチャ読み込み関数
		const wchar_t* fileName,	//モデルファイルのパス
		Mesh& mesh,		//メッシュデータ構造体への参照
		const aiMaterial* src		//Assimpのメッシュ構造体へのポインタ
	);

	static Vector4 GetMaterialColor( //マテリアルカラー取得関数
		const aiMaterial* src	//Assimpのマテリアル構造体へのポインタ
		);
};