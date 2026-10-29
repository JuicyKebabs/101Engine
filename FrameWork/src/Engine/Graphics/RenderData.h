#pragma once
#include "Engine/Core/Math/Math.h"
#include "Engine/Graphics/VertexTypes.h"
#include <vector>
#include <string>
#include <unordered_map>

// CPU primitive/model vocabulary. Native PSO keys and old render queues were
// removed; file import is supplied separately by RendererAssets.
// Type of Billboard
enum class BillboardType
{
	None,			//ビルボードなし
	Spherical,		//全軸ビルボード
	Cylindrical,	//円柱ビルボード
};

//=======================================================================================================
// Mesh and Model related data structures
//=======================================================================================================
// Mesh data structure
struct Mesh
{
	std::vector<Vertex> vertices;	// Vertex data array
	size_t vertexCount = 0;			// Vertex count
	std::vector<uint32_t> indices;	// Index data array
	size_t indexCount = 0;			// Index count
	std::wstring texPath;			// Texture file name
	Vector4 materialColor			// Material color (RGBA)
	{
		1.0f,	// Diffuse color R
		1.0f,	// Diffuse color G
		1.0f,	// Diffuse color B
		1.0f	// Diffuse color A
	};
	Vector3 boundsCenter{};				// Bounding sphere center (for sorting)
	float boundsRadius = 0.0f;			// Bounding sphere radius (for sorting)
};

// Bone data structure
struct Bone
{
	std::wstring name;			// Bone name
	int parentIndex = -1;		// Parent bone index (-1 if root)
	DirectX::XMMATRIX offset;	// Offset matrix
	int nodeIndex = -1;			// Node index in the model's node hierarchy
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

using Model = std::vector<Mesh>;	// Model is a collection of meshes

//mesh type enumeration
enum class DefaultMesh
{
	Quad,		//quad plane
	Cube,		//cube
	Circle,		//circle plane
	Sphere,		//sphere
	Capsule,	//capsule
	Cylinder,	//cylinder
};

//=======================
//四角平面
//=======================
//四角平面の頂点データ
static const Vertex QuadVertices[4] =
{
	{{-0.5f, 0.5f, 0.0f}, {0,0,1}, {0,0}, {1,0,0}, {1,1,1,1}},	//頂点0
	{{ 0.5f, 0.5f, 0.0f}, {0,0,1}, {1,0}, {1,0,0}, {1,1,1,1}},	//頂点1
	{{ 0.5f,-0.5f, 0.0f}, {0,0,1}, {1,1}, {1,0,0}, {1,1,1,1}},	//頂点2
	{{-0.5f,-0.5f, 0.0f}, {0,0,1}, {0,1}, {1,0,0}, {1,1,1,1}},	//頂点3
};

//四角平面のインデックスデータ
inline uint32_t QuadIndices[6] =
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
static const Vertex CubeVertices[24] =
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
	{{-0.5,  0.5, -0.5}, {0,1,0}, {0,0}, {1,0,0}, {1,1,1,1}},	//頂点4
	{{ 0.5,  0.5, -0.5}, {0,1,0}, {1,0}, {1,0,0}, {1,1,1,1}},	//頂点5
	{{ 0.5,  0.5,  0.5}, {0,1,0}, {1,1}, {1,0,0}, {1,1,1,1}},	//頂点1
	{{-0.5,  0.5,  0.5}, {0,1,0}, {0,1}, {1,0,0}, {1,1,1,1}},	//頂点0

	// -Y
	{{-0.5, -0.5,  0.5}, {0,-1,0}, {0,0}, {1,0,0}, {1,1,1,1}},	//頂点3
	{{ 0.5, -0.5,  0.5}, {0,-1,0}, {1,0}, {1,0,0}, {1,1,1,1}},	//頂点2
	{{ 0.5, -0.5, -0.5}, {0,-1,0}, {1,1}, {1,0,0}, {1,1,1,1}},	//頂点6
	{{-0.5, -0.5, -0.5}, {0,-1,0}, {0,1}, {1,0,0}, {1,1,1,1}},	//頂点7
};

//立方体のインデックスデータ
inline constexpr uint32_t CubeIndices[36] =
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

////カプセルの描画情報追加関数
//void AppendCapsuleRenderInfos(
//	const CapsuleVisualDesc& desc,				//カプセル描画情報記述子
//	const DirectX::XMFLOAT3& position,			//位置
//	const DirectX::XMFLOAT3& scale,				//スケール
//	const DirectX::XMFLOAT3& rotEuler,			//回転Euler角
//	const DirectX::XMFLOAT4& color,				//色
//	std::vector<WorldRenderInfo>& infos,	//入力元描画情報配列
//	std::vector<WorldRenderInfo>& out	//出力先描画情報配列
//);

//=======================
//円柱
//=======================
//円柱のメッシュデータ作成関数
Model MakeCylinderModel(int slice = 32, int stacks = 16);

//メッシュデータ取得関数
inline Model GetDefaultModel(DefaultMesh type)
{
	//メッシュタイプに応じたメッシュデータを返す
	switch (type)
	{
	case DefaultMesh::Quad: return MakeQuadModel();			//四角平面
	case DefaultMesh::Cube: return MakeCubeModel();			//立方体
	case DefaultMesh::Sphere: return MakeSphereModel();		//球体
	case DefaultMesh::Circle: return MakeCircleModel();		//円形平面
	case DefaultMesh::Capsule: return MakeCapsuleModel();	//カプセル
	case DefaultMesh::Cylinder: return MakeCylinderModel();	//円柱
	default:   return {};						//その他
	}
}
