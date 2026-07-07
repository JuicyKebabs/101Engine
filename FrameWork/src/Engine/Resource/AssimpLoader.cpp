#include "AssimpLoader.h"
#include "Engine/Component/AssimpNodeTransformAnim.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/version.h>
#include <DirectXMath.h>
#include <d3dx12.h>
#include <filesystem>

namespace fs = std::filesystem;

//ディレクトリパス取得関数
std::wstring GetDirectoryPath(const std::wstring& origin)
{
	fs::path p(origin);					//ファイルパスをpathオブジェクトに変換
	return p.parent_path().wstring();	//親ディレクトリパスを返す
}

//std::wstring(ワイド文字列)からstd::string(マルチバイト文字列)を得る
std::string ToUTF8(const std::wstring& value)
{
	auto length = WideCharToMultiByte(CP_UTF8, 0U, value.data(), -1, nullptr, 0, nullptr, nullptr);
	auto buffer = new char[length];

	WideCharToMultiByte(CP_UTF8, 0U, value.data(), -1, buffer, length, nullptr, nullptr);

	std::string result(buffer);
	delete[] buffer;
	buffer = nullptr;

	return result;
}

//std::string(マルチバイト文字列)からstd::wstring(ワイド文字列)を得る
std::wstring ToWideString(const std::string& str)
{
	auto num1 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);

	std::wstring wstr;
	wstr.resize(num1);

	auto num2 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, &wstr[0], num1);

	assert(num1 == num2);
	return wstr;
}

//モデル読み込み関数
bool AssimpLoader::Load(ImportSettings settings)
{
	//ファイル名がnullptrの場合はfalseを返す 
	if (settings.fileName == nullptr)
	{
		return false;
	}

	auto& meshes = settings.meshs;		//メッシュデータ配列への参照
	auto inverseU = settings.inverseU;	//Uを反転するかどうか
	auto inverseV = settings.inverseV;	//Vを反転するかどうか

	auto path = ToUTF8(settings.fileName); //ファイルパスをマルチバイト文字列に変換

	Assimp::Importer importer;	//Assimpのインポーター生成
	int flag = 0;   //インポート設定フラグ
	flag |= aiProcess_Triangulate;				//三角形化
	flag |= aiProcess_CalcTangentSpace;			//接線空間の計算
	flag |= aiProcess_GenSmoothNormals;			//スムーズシェーディング用法線の計算
	flag |= aiProcess_GenUVCoords;				//UV座標の生成
	flag |= aiProcess_RemoveRedundantMaterials;	//冗長なマテリアルの削除
	flag |= aiProcess_OptimizeMeshes;			//メッシュの最適化
	flag |= aiProcess_LimitBoneWeights;			//ボーンウェイトの制限
	flag |= aiProcess_ConvertToLeftHanded;		//左手座標系に変換

	auto scene = importer.ReadFile(path, flag); //モデル読み込み
	auto aiTexture = scene->mNumTextures; //埋め込みテクスチャ数

	if (!scene)
	{
		//読み込み失敗時のエラーメッセージ出力
		printf(importer.GetErrorString());
		printf("\n");
		return false;
	}

	//メッシュデータ配列分のメモリを確保
	meshes.clear();						//メッシュデータ配列をクリア
	meshes.resize(scene->mNumMeshes);	//メッシュデータ配列のリサイズ

	//メッシュデータ配列に要素を追加
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		//メッシュの読み込み
		const auto pMesh = scene->mMeshes[i];			//メッシュ構造体へのポインタ
		LoadMesh(meshes[i], pMesh, inverseU, inverseV);	//メッシュ読み込み関数の呼び出し

		//テクスチャの読み込み
		const auto pMaterial = scene->mMaterials[pMesh->mMaterialIndex];	//マテリアル構造体へのポインタ
		LoadTexture(settings.fileName, meshes[i], pMaterial);				//テクスチャ読み込み関数の呼び出し
	
		BuildNodeTree(scene, meshes[i].nodeAnimAsset);	//ノードツリー構築関数の呼び出し
		BuildClip0(scene, meshes[i].nodeAnimAsset);		//クリップ0構築関数の呼び出し
	}

	scene = nullptr; //シーン情報の解放

	return true;
}

//メッシュ読み込み関数
void AssimpLoader::LoadMesh(
	Mesh& dst,			//メッシュデータ構造体への参照
	const aiMesh* src,	//Assimpのメッシュ構造体へのポインタ
	bool inverseU,		//Uを反転するかどうか
	bool inverseV		//Vを反転するかどうか
)
{
	aiVector3D zero3D(0.0f, 0.0f, 0.0f);			//ゼロベクトル
	aiColor4D zeroColor(1.0f, 1.0f, 1.0f, 1.0f);	//ゼロカラー

	dst.vertices.resize(src->mNumVertices);	//頂点データ配列のリサイズ

	Vector3 minPos(FLT_MAX, FLT_MAX, FLT_MAX);		//最小座標
	Vector3 maxPos(-FLT_MAX, -FLT_MAX, -FLT_MAX);	//最大座標

	//頂点データの格納
	for (auto i = 0u; i < src->mNumVertices; ++i)
	{
		//各頂点データのポインタを取得
		auto position = &(src->mVertices[i]);
		auto normal = &(src->mNormals[i]);
		auto uv = (src->HasTextureCoords(0)) ? &(src->mTextureCoords[0][i]) : &zero3D;
		auto tangent = (src->HasTangentsAndBitangents()) ? &(src->mTangents[i]) : &zero3D;
		auto color = (src->HasVertexColors(0)) ? &(src->mColors[0][i]) : &zeroColor;

		//UV反転
		if (inverseU)
		{
			uv->x = 1 - uv->x;
		}
		if (inverseV)
		{
			uv->y = 1 - uv->y;
		}

		//頂点データの格納
		Vertex vertex = {};
		vertex.position = Vector3(position->x, position->y, position->z);
		vertex.normal = Vector3(normal->x, normal->y, normal->z);
		vertex.uv = Vector2(uv->x, uv->y);
		vertex.tangent = Vector3(tangent->x, tangent->y, tangent->z);
		vertex.color = Vector4(color->r, color->g, color->b, color->a);

		//頂点データ配列に格納
		dst.vertices[i] = vertex;

		//バウンディングボックスの更新
		minPos.x = std::min(minPos.x, vertex.position.x);
		minPos.y = std::min(minPos.y, vertex.position.y);
		minPos.z = std::min(minPos.z, vertex.position.z);
		maxPos.x = std::max(maxPos.x, vertex.position.x);
		maxPos.y = std::max(maxPos.y, vertex.position.y);
		maxPos.z = std::max(maxPos.z, vertex.position.z);
	}

	dst.boundsCenter = Vector3(
		(minPos.x + maxPos.x) / 2.0f,
		(minPos.y + maxPos.y) / 2.0f,
		(minPos.z + maxPos.z) / 2.0f
	);
	dst.boundsRadius = sqrtf(
		(maxPos.x - minPos.x) * (maxPos.x - minPos.x) +
		(maxPos.y - minPos.y) * (maxPos.y - minPos.y) +
		(maxPos.z - minPos.z) * (maxPos.z - minPos.z)
	) / 2.0f;

	//頂点数の設定
	dst.vertexCount = src->mNumVertices;

	//インデックスデータ配列のリサイズ
	dst.indices.resize(src->mNumFaces * 3);

	//インデックスデータの格納
	for (auto i = 0u; i < src->mNumFaces; ++i)
	{
		const auto& face = src->mFaces[i];

		dst.indices[i * 3 + 0] = face.mIndices[0];
		dst.indices[i * 3 + 1] = face.mIndices[1];
		dst.indices[i * 3 + 2] = face.mIndices[2];
	}

	//インデックス数の設定
	dst.indexCount = src->mNumFaces * 3;
}

//テクスチャ読み込み関数
void AssimpLoader::LoadTexture(
	const wchar_t* fileName,	//モデルファイルのパス
	Mesh& dst,					//メッシュデータ構造体への参照
	const aiMaterial* src		//Assimpのメッシュ構造体へのポインタ
)
{
	dst.materialColor = GetMaterialColor(src);	//マテリアルカラー取得関数の呼び出し

	aiString path;	//テクスチャパス格納用aiString

	//拡散反射テクスチャのパスを取得
	if (src->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) == AI_SUCCESS)
	{//取得成功時
		// テクスチャパスは相対パスで入っているので、ファイルの場所とくっつける
		auto dir = GetDirectoryPath(fileName);	//モデルファイルのディレクトリパスを取得
		auto file = std::string(path.C_Str());	//AssimpのaiStringをstd::stringに変換

		std::filesystem::path p(dir);						//ディレクトリパスをpathオブジェクトに変換
		dst.texPath = (p / ToWideString(file)).wstring();	//フルパスを取得してワイド文字列に変換

	}
	else
	{//取得失敗時
		dst.texPath.clear();	//テクスチャパスをクリア
	}
}

//マテリアルカラー取得関数
Vector4 AssimpLoader::GetMaterialColor(const aiMaterial* src)
{
	aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);	//マテリアルカラー格納用aiColor4D

	if (src->Get("$clr.base", 0, 0, color) != aiReturn_SUCCESS)
	{
		aiGetMaterialColor(src, AI_MATKEY_COLOR_DIFFUSE, &color);
	}

	float opacity = 1.0f;
	src->Get(AI_MATKEY_OPACITY, opacity);
	color.a *= opacity;

	return Vector4(
		color.r,
		color.g,
		color.b,
		color.a
	);
}
