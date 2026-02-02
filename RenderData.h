#pragma once
#include <DirectXMath.h>
#include "AssimpNodeTransformAnim.h"
#include "SharedStruct.h"
#include <vector>

//前方宣言
class Renderer;
class TextureManager;
class MeshManager;
class MeshGPU;
struct NodeAnimationAsset;

//=======================================================================================================
//列挙体群
//=======================================================================================================
//ブレンドモード列挙体
enum BLEND_MODE
{
	BLEND_OPAQUE,		//不透明
	BLEND_MASKED,		//マスク
	BLEND_TRANSPARENT,	//透明
	BLEND_MAX			//最大数
};

//ビルボードタイプ
enum BILLBOARD_TYPE
{
	BILLBOARD_NONE,			//ビルボードなし
	BILLBOARD_SPHERICAL,	//全軸ビルボード
	BILLBOARD_FIX_X,		//X軸のみ
	BILLBOARD_FIX_Y,		//Y軸のみ
	BILLBOARD_FIX_Z,		//Z軸のみ
};

//=======================================================================================================
//描画情報構造体群
//=======================================================================================================
//共通描画記述構造体
struct CommonRenderDesc
{
	MeshGPU* pMeshGPU = nullptr;						//メッシュデータ
	uint32_t srvIndex = UINT32_MAX;						//SRVインデックス(テクスチャ)
	DirectX::XMFLOAT4 color = { 1,1,1,1 };				//表示色
	DirectX::XMFLOAT4 uvRect{ 0.0f, 0.0f, 1.0f, 1.0f };	//UV矩形
	BLEND_MODE blendMode = BLEND_MODE::BLEND_OPAQUE;	//ブレンドモード
};

// Render information structure for world space
struct WorldRenderInfo
{
	CommonRenderDesc common{};				// Common render description structure
	DirectX::XMMATRIX world = {};			// World matrix
	UINT startIndex = 0;					// Start index
	INT  baseVertex = 0;					// Base vertex
	DirectX::XMFLOAT3 position{};			// Position
	DirectX::XMFLOAT3 scale{};				// Scale
	bool lightingEnabled = true;			// Lighting enabled flag
	BILLBOARD_TYPE billboardType
		= BILLBOARD_TYPE::BILLBOARD_NONE;	// Billboard type

	NodeAnimationAsset* pNodeAnimAsset = nullptr;	// Pointer to node animation asset
};

//描画情報構造体配列型
using WorldRenderModel = std::vector<WorldRenderInfo>;

//エフェクト描画情報構造体
struct EffectRenderInfo
{
	CommonRenderDesc common;	//共通描画記述構造体
	DirectX::XMFLOAT3 center{};	//座標
	DirectX::XMFLOAT2 size{};	//スケール
};

//エフェクト描画情報構造体配列型
using EffectRenderModel = std::vector<EffectRenderInfo>;

//=======================================================================================================
//メッシュ・モデルデータ構造体
//=======================================================================================================
//メッシュデータ構造体
struct Mesh
{
	std::vector<Vertex> vertices;		//頂点データ配列
	size_t vertexCount = 0;				//頂点数
	std::vector<uint32_t> indices;		//インデックスデータ配列
	size_t indexCount = 0;				//インデックス数
	std::wstring texPath;				//テクスチャのファイル名
	DirectX::XMFLOAT4 materialColor		//材質色(RGBA)
	{	
		1.0f,	//拡散反射色R
		1.0f,	//拡散反射色G
		1.0f,	//拡散反射色B
		1.0f	//拡散反射色A
	};
	NodeAnimationAsset nodeAnimAsset{};	// ノードアニメーション資産
};

// Bone data structure
struct Bone
{
	std::wstring name;				// Bone name
	int parentIndex = -1;			// Parent bone index (-1 if root)
	DirectX::XMMATRIX offset;		// Offset matrix
	int nodeIndex = -1;				// Node index in the model's node hierarchy
};

// Skeleton data structure
struct Skeleton
{
	std::vector<Bone> bones;						// Bone array
	std::unordered_map<std::wstring, int> boneMap;	// Map from bone name to index
};

struct AnimationClip
{
	std::wstring name;	// Animation clip name
	float duration;		// Duration in seconds
	float ticksPerSecond; // Ticks per second
};

//モデルデータ構造体
struct Model
{
	std::vector<Mesh> meshes;	//メッシュデータ配列
};

//mesh type enumeration
enum MESH_TYPE
{
	IMPORT,		//imported model
	QUAD,		//quad plane
	CUBE,		//cube
	CIRCLE,		//circle plane
	SPHERE,		//sphere
	CAPSULE,	//capsule
	CYLINDER,	//cylinder
};

//=======================
//四角平面
//=======================
//四角平面の頂点データ
inline constexpr Vertex QuadVertices[4] =
{
	{{-0.5f,  0.5f, 0.f},{0,0,1},{0,0},{1,0,0},{1,1,1,1}},	//頂点0
	{{ 0.5f,  0.5f, 0.f},{0,0,1},{1,0},{1,0,0},{1,1,1,1}},	//頂点1
	{{ 0.5f, -0.5f, 0.f},{0,0,1},{1,1},{1,0,0},{1,1,1,1}},	//頂点2
	{{-0.5f, -0.5f, 0.f},{0,0,1},{0,1},{1,0,0},{1,1,1,1}},	//頂点3
};

//四角平面のインデックスデータ
inline constexpr uint32_t QuadIndices[6] =
{
	0,1,2,	//三角形1
	0,2,3	//三角形2
};

//四角平面のメッシュデータ作成関数
Model MakeQuadModel();

//=======================
//立方体
//=======================
//立方体の頂点データ(24頂点)
inline constexpr Vertex CubeVertices[24] =
{
	// +Z
	{{-0.5,  0.5,  0.5}, {0,0,1}, {0,0}, {1,0,0}, {1,1,1,1}},	//頂点0
	{{ 0.5,  0.5,  0.5}, {0,0,1}, {1,0}, {1,0,0}, {1,1,1,1}},	//頂点1
	{{ 0.5, -0.5,  0.5}, {0,0,1}, {1,1}, {1,0,0}, {1,1,1,1}},	//頂点2
	{{-0.5, -0.5,  0.5}, {0,0,1}, {0,1}, {1,0,0}, {1,1,1,1}},	//頂点3

	// -Z
	{{ 0.5,  0.5, -0.5}, {0,0,-1}, {0,0}, {-1,0,0}, {1,1,1,1}},	//頂点4
	{{-0.5,  0.5, -0.5}, {0,0,-1}, {1,0}, {-1,0,0}, {1,1,1,1}},	//頂点5
	{{-0.5, -0.5, -0.5}, {0,0,-1}, {1,1}, {-1,0,0}, {1,1,1,1}},	//頂点6
	{{ 0.5, -0.5, -0.5}, {0,0,-1}, {0,1}, {-1,0,0}, {1,1,1,1}},	//頂点7

	// +X
	{{ 0.5,  0.5,  0.5}, {1,0,0}, {0,0}, {0,0,-1}, {1,1,1,1}},	//頂点1
	{{ 0.5,  0.5, -0.5}, {1,0,0}, {1,0}, {0,0,-1}, {1,1,1,1}},	//頂点5
	{{ 0.5, -0.5, -0.5}, {1,0,0}, {1,1}, {0,0,-1}, {1,1,1,1}},	//頂点6
	{{ 0.5, -0.5,  0.5}, {1,0,0}, {0,1}, {0,0,-1}, {1,1,1,1}},	//頂点2

	// -X
	{{-0.5,  0.5, -0.5}, {-1,0,0}, {0,0}, {0,0,1}, {1,1,1,1}},	//頂点4
	{{-0.5,  0.5,  0.5}, {-1,0,0}, {1,0}, {0,0,1}, {1,1,1,1}},	//頂点0
	{{-0.5, -0.5,  0.5}, {-1,0,0}, {1,1}, {0,0,1}, {1,1,1,1}},	//頂点3
	{{-0.5, -0.5, -0.5}, {-1,0,0}, {0,1}, {0,0,1}, {1,1,1,1}},	//頂点7

	// +Y
	{{-0.5,  0.5, -0.5}, {0,1,0}, {0,0}, {1,0,0}, {1,1,1,1}},		//頂点4
	{{ 0.5,  0.5, -0.5}, {0,1,0}, {1,0}, {1,0,0}, {1,1,1,1}},		//頂点5
	{{ 0.5,  0.5,  0.5}, {0,1,0}, {1,1}, {1,0,0}, {1,1,1,1}},		//頂点1
	{{-0.5,  0.5,  0.5}, {0,1,0}, {0,1}, {1,0,0}, {1,1,1,1}},		//頂点0

	// -Y
	{{-0.5, -0.5,  0.5}, {0,-1,0}, {0,0}, {1,0,0}, {1,1,1,1}},	//頂点3
	{{ 0.5, -0.5,  0.5}, {0,-1,0}, {1,0}, {1,0,0}, {1,1,1,1}},	//頂点2
	{{ 0.5, -0.5, -0.5}, {0,-1,0}, {1,1}, {1,0,0}, {1,1,1,1}},	//頂点6
	{{-0.5, -0.5, -0.5}, {0,-1,0}, {0,1}, {1,0,0}, {1,1,1,1}},	//頂点7
};

//立方体のインデックスデータ
inline constexpr uint32_t CubeIndices[42] =
{
	// +Z
	0,1,2,  0,2,3,			//三角形1、2
	// -Z
	4,6,5,  4,7,6,			//三角形3、4
	// +X
	8,9,10,  8,10,11,		//三角形5、6
	// -X
	12,13,14,  12,14,15,	//三角形7、8
	// +Y
	16,17,18,  16,18,19,	//三角形9、10
	// -Y
	20,21,22,  20,22,23		//三角形11、12
};

//立方体のメッシュデータ作成関数
Model MakeCubeModel();

//=======================
//円形平面
//=======================
//円形平面のメッシュデータ作成関数
Model MakeCircleModel(int slice = 32);

//=======================
//球体
//=======================
//球体のメッシュデータ作成関数
Model MakeSphereModel(int slice = 32, int stacks = 16);

//=======================
//カプセル
//=======================
//カプセルのメッシュデータ作成関数
Model MakeCapsuleModel(int slice = 32, int stacks = 16);

//カプセルのビジュアル記述構造体
struct CapsuleVisualDesc
{
	float baseRadius = 0.5f;		//底面半径
	float basehalfHeight = 0.5f;	//半分の高さ
};

//カプセルの描画情報追加関数
void AppendCapsuleRenderInfos(
	const CapsuleVisualDesc& desc,				//カプセル描画情報記述子
	const DirectX::XMFLOAT3& position,			//位置
	const DirectX::XMFLOAT3& scale,				//スケール
	const DirectX::XMFLOAT3& rotEuler,			//回転Euler角
	const DirectX::XMFLOAT4& color,				//色
	std::vector<WorldRenderInfo>& infos,	//入力元描画情報配列
	std::vector<WorldRenderInfo>& out	//出力先描画情報配列
);

//=======================
//円柱
//=======================
//円柱のメッシュデータ作成関数
Model MakeCylinderModel(int slice = 32, int stacks = 16);

//メッシュデータ取得関数
inline Model GetModel(MESH_TYPE type)
{
	//メッシュタイプに応じたメッシュデータを返す
	switch (type)
	{
	case QUAD: return MakeQuadModel();			//四角平面
	case CUBE: return MakeCubeModel();			//立方体
	case SPHERE: return MakeSphereModel();		//球体
	case CIRCLE: return MakeCircleModel();		//円形平面
	case CAPSULE: return MakeCapsuleModel();	//カプセル
	case CYLINDER: return MakeCylinderModel();	//円柱
	default:   return {};						//その他
	}
}

//=======================================================================================================
//描画情報作成関数群
//=======================================================================================================
//モデルデータ又はテクスチャファイルから描画情報を作成する関数
void CreateRenderInfo(
	TextureManager& textureManager,			//テクスチャマネージャへの参照
	MeshManager& meshManager,				//メッシュマネージャへの参照
	std::vector<WorldRenderInfo>* pInfo,	//描画情報構造体配列へのポインタ
	MESH_TYPE mType,						//メッシュタイプ
	BLEND_MODE mode,						//ブレンドモード
	const wchar_t* path,					//モデルデータ又はテクスチャファイルのパス
	bool lightEneble = true,				//ライト有効or無効
	BILLBOARD_TYPE bType = BILLBOARD_NONE,	//ビルボードタイプ
	bool inverseU = false,					//Uを反転するかどうか(モデルデータの場合のみ有効)
	bool inverseV = false					//Vを反転するかどうか(モデルデータの場合のみ有効)
);

//FBXファイルから描画情報を作成する関数
void CreateRenderInfoFromFBX(
	TextureManager& textureManager,			//テクスチャマネージャへの参照
	MeshManager& meshManager,				//メッシュマネージャへの参照
	std::vector<WorldRenderInfo>* pInfo,	//描画情報構造体配列へのポインタ
	BLEND_MODE mode,						//ブレンドモード
	const wchar_t* path,					//モデルファイルのパス
	bool lightEneble,						//ライト有効or無効
	BILLBOARD_TYPE bType = BILLBOARD_NONE,	//ビルボードタイプ
	bool inverseU = false,					//Uを反転するかどうか
	bool inverseV = false					//Vを反転するかどうか
);

//デフォルトのメッシュデータから描画情報を作成する関数
void CreateRenderInfoFromDefaultMesh(
	TextureManager& textureManager,			//テクスチャマネージャへの参照
	MeshManager& meshManager,				//メッシュマネージャへの参照
	std::vector<WorldRenderInfo>* pInfo,	//描画情報構造体配列へのポインタ
	MESH_TYPE type,							//メッシュタイプ
	BLEND_MODE mode,						//ブレンドモード
	const wchar_t* path,					//テクスチャのファイル名
	bool lightEneble,						//ライト有効or無効
	BILLBOARD_TYPE bType = BILLBOARD_NONE	//ビルボードタイプ
);

void CreateEffectRenderInfo(
	TextureManager& textureManager,			//テクスチャマネージャへの参照
	MeshManager& meshManager,				//メッシュマネージャへの参照
	std::vector<EffectRenderInfo>* pInfo,	//エフェクト描画情報構造体配列へのポインタ
	MESH_TYPE type,				//メッシュタイプ
	BLEND_MODE mode,						//ブレンドモード
	const wchar_t* path						//テクスチャのファイル名
);

//メッシュデータから描画情報を構築する関数
CommonRenderDesc CreateRenderInfoFromMeshData(
	TextureManager& textureManager,			//テクスチャマネージャへの参照
	MeshManager& meshManager,				//メッシュマネージャへの参照
	Mesh& mesh,					//メッシュデータ構造体への参照
	BLEND_MODE mode,						//ブレンドモード
	BILLBOARD_TYPE bType = BILLBOARD_NONE	//ビルボードタイプ
);

//描画情報配列とジオメトリ情報から提出用描画情報配列を構築する関数
WorldRenderModel BuildRenderInfoForSubmit(
	const WorldRenderModel& input,
	MESH_TYPE meshType = MESH_TYPE::QUAD,
	const DirectX::XMFLOAT3& position = { 0.0f, 0.0f, 0.0f },
	const DirectX::XMFLOAT3& scale = { 1.0f, 1.0f, 1.0f },
	const DirectX::XMFLOAT3& rotation = { 0.0f, 0.0f, 0.0f },
	const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f },
	const TexSplitInfo& texSplitInfo = {}
);