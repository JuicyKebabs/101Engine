#pragma once
#include <DirectXMath.h>
#include "Engine/Component/AssimpNodeTransformAnim.h"
#include "Engine/Core/Utility/SharedStruct.h"
#include "Engine/Core/Math/Math.h"
#include <vector>

// Forward declarations
class Renderer;
class TextureManager;
class MeshManager;
class MeshGPU;
struct NodeAnimationAsset;

//=======================================================================================================
// Pipeline State Object (PSO) key structure,
// and related enumerations for shader management and rendering configurations
//=======================================================================================================

//-------------------------------------------------------------------------------------
// File IDs for shaders (used to identify which shader file to use)
//-------------------------------------------------------------------------------------
// Enumeration for vertex shader IDs
enum class VS_FILE_ID
{
	Basic = 0,
};
// Enumeration for pixel shader IDs
enum class PS_FILE_ID
{
	Basic = 0,
};

//-------------------------------------------------------------------------------------
// Entry point IDs for shaders (used to identify which entry point in the shader file to use)
//-------------------------------------------------------------------------------------
// Enumeration for vertex shader entry points
enum class VS_ENTRY_ID : uint16_t
{
	Basic = 0,
	PostEffect = 1,
};
// Enumeration for pixel shader entry points
enum class PS_ENTRY_ID : uint16_t
{
	Basic = 0,
	PostEffect = 1,
};

//-------------------------------------------------------------------------------------
// Defines for shaders (used to specify shader variants based on compile-time options)
//-------------------------------------------------------------------------------------
// Vertex shader defines (using bit flags)
enum class VS_DEFINE : uint64_t
{
	None = 0,
	Test = 1ull << 1,
};
// Pixel shader defines (using bit flags)
enum class PS_DEFINE : uint64_t
{
	None = 0,
	UseMask = 1ull << 1,
	MultiplyAlphaControll = 1ull << 2,
	UseLighting = 1ull << 3,
};
// Common shader defines (using bit flags)
enum class COMMON_SHADER_DEFINE : uint64_t
{
	None = 0,
	Test = 1ull << 1,
};

//-------------------------------------------------------------------------------------
// Shader key structure for caching compiled shaders, 
// combining file ID, entry point, and defines
//-------------------------------------------------------------------------------------
struct VS_KEY
{
	VS_FILE_ID fileID = VS_FILE_ID::Basic;		//Vertex shader file ID
	VS_ENTRY_ID entryID = VS_ENTRY_ID::Basic;	//Vertex shader entry point
	uint64_t defines = 0;						//Vertex shader defines

	bool operator == (const VS_KEY& other) const
	{
		return 
			fileID == other.fileID &&
			entryID == other.entryID &&
			defines == other.defines;
	}
};
struct PS_KEY
{
	PS_FILE_ID fileID = PS_FILE_ID::Basic;		//Pixel shader file ID
	PS_ENTRY_ID entryID = PS_ENTRY_ID::Basic;	//Pixel shader entry point
	uint64_t defines = 0;						//Pixel shader defines

	bool operator == (const PS_KEY& other) const
	{
		return 
			fileID == other.fileID &&
			entryID == other.entryID &&
			defines == other.defines;
	}
};

//-------------------------------------------------------------------------------------
// Enumerations for rendering states (blend modes, depth stencil modes, culling modes)
//-------------------------------------------------------------------------------------
// Blend modes
enum class BlendMode
{
	None,
	Opaque,
	Alpha,
	AddAlpha,
	Add,
	Multiply,
};
// Depth stencil modes
enum class DepthMode
{
	None,
	Disable,
	TestWrite,
	TestNoWrite,
};
// Culling modes
enum class CullMode
{
	None,
	Front,
	Back,
};

// Render target formats
enum class RenderTargetFormat
{
	None,
	LDR = 0,
	HDR = 1,
};

// Pipeline State Object (PSO) key structure
struct PSOKey
{
	VS_KEY vsKey;												// Vertex shader key
	PS_KEY psKey;												// Pixel shader key
	uint64_t commonDefines = 0;									// Common shader defines
	BlendMode  blend = BlendMode::None;							// Blend mode
	DepthMode  depth = DepthMode::None;							// Depth stencil mode
	CullMode  cull = CullMode::None;							// Culling mode
	RenderTargetFormat rtvFormat = RenderTargetFormat::None;	// Render target format

	// Equality operators for PSOKey
	bool operator == (const PSOKey& other) const
	{
		return 
			vsKey == other.vsKey &&
			psKey == other.psKey &&
			commonDefines == other.commonDefines &&
			blend == other.blend &&
			depth == other.depth &&
			cull == other.cull &&
			rtvFormat == other.rtvFormat;
	}
	bool operator != (const PSOKey& other) const
	{
		return !(*this == other);
	}

	// Helper functions to create modified PSOKeys with additional defines
	PSOKey WithLighting() const {
		PSOKey k = *this;
		k.psKey.defines |= static_cast<uint64_t>(PS_DEFINE::UseLighting);
		return k;
	}

	// Add multiple vertex shader defines at once
	PSOKey AddVSDefines(std::initializer_list<VS_DEFINE> defines) const {
		PSOKey k = *this;
		for (auto d : defines) {
			k.vsKey.defines |= static_cast<uint64_t>(d);
		}
		return k;
	}
	// Add multiple pixel shader defines at once
	PSOKey AddPSDefines(std::initializer_list<PS_DEFINE> defines) const {
		PSOKey k = *this;
		for (auto d : defines) {
			k.psKey.defines |= static_cast<uint64_t>(d);
		}
		return k;
	}
	// Add multiple common shader defines at once
	PSOKey AddCommonDefines(std::initializer_list<COMMON_SHADER_DEFINE> defines) const {
		PSOKey k = *this;
		for (auto d : defines) {
			k.commonDefines |= static_cast<uint64_t>(d);
		}
		return k;
	}

	// Get combined defines for vertex shader (common + specific)
	uint64_t GetCombinedDefinesVS() const {
		return vsKey.defines | commonDefines;
	}
	// Get combined defines for pixel shader (common + specific)
	uint64_t GetCombinedDefinesPS() const {
		return psKey.defines | commonDefines;
	}
};

// Predefined PSO keys for common rendering configurations
inline constexpr PSOKey PSO_KEY_OPAQUE{ VS_KEY{}, PS_KEY{}, 0, BlendMode::Opaque, DepthMode::TestWrite, CullMode::None };
inline constexpr PSOKey PSO_KEY_TRANSPARENT { VS_KEY{}, PS_KEY{}, 0, BlendMode::Alpha, DepthMode::TestNoWrite, CullMode::None };
inline constexpr PSOKey PSO_KEY_MASKED { VS_KEY{}, PS_KEY{.defines = static_cast<uint64_t>(PS_DEFINE::UseMask)}, 0, BlendMode::Opaque, DepthMode::TestWrite, CullMode::None };
inline constexpr PSOKey PSO_KEY_ADDITIVE { VS_KEY{}, PS_KEY{}, 0, BlendMode::AddAlpha, DepthMode::TestNoWrite, CullMode::None };
inline constexpr PSOKey PSO_KEY_MULTIPLY { VS_KEY{}, PS_KEY{.defines = static_cast<uint64_t>(PS_DEFINE::MultiplyAlphaControll)}, 0, BlendMode::Multiply, DepthMode::TestNoWrite, CullMode::None };

// Hash function for PSOKey to be used in unordered_map
struct PSOKeyHash
{
	size_t operator()(const PSOKey& k) const noexcept
	{
		size_t h1 = std::hash<VS_FILE_ID>{}(k.vsKey.fileID);
		size_t h2 = std::hash<PS_FILE_ID>{}(k.psKey.fileID);
		size_t h3 = std::hash<VS_ENTRY_ID>{}(k.vsKey.entryID);
		size_t h4 = std::hash<PS_ENTRY_ID>{}(k.psKey.entryID);
		size_t h5 = std::hash<int>{}(static_cast<int>(k.blend));
		size_t h6 = std::hash<int>{}(static_cast<int>(k.depth));
		size_t h7 = std::hash<int>{}(static_cast<int>(k.cull));
		size_t h8 = std::hash<int>{}(static_cast<int>(k.rtvFormat));
		size_t h9 = std::hash<uint64_t>{}(k.vsKey.defines);
		size_t h10 = std::hash<uint64_t>{}(k.psKey.defines);
		size_t h11 = std::hash<uint64_t>{}(k.commonDefines);
		return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^ (h7 << 6) ^ (h8 << 7) ^ (h9 << 8) ^ (h10 << 9) ^ (h11 << 10);
	}
};

// Render queue enumeration
enum class RenderQueue
{
	Invalied = -1,
	Opaque = 0,
	Additive = 1,
	Transparent = 2,
};

// Helper function to determine render queue based on blend mode
static inline RenderQueue GetRenderQueueFromBlendMode(BlendMode blendMode)
{
	return (blendMode == BlendMode::Opaque) ? RenderQueue::Opaque : RenderQueue::Transparent;
}

//=======================================================================================================
//描画情報構造体群
//=======================================================================================================

// Type of Billboard
enum class BillboardType
{
	None,			//ビルボードなし
	Spherical,	//全軸ビルボード
	FixedX,		//X軸固定ビルボード
	FixedY,		//Y軸固定ビルボード
	FixedZ,		//Z軸固定ビルボード
};

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
	Vector3 boundsCenter{};				//バウンディングスフィアの中心座標(ソート用)
	float boundsRadius = 0.0f;			//バウンディングスフィアの半径(ソート用)
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

using Model = std::vector<Mesh>;	//モデルデータ構造体(複数のメッシュからなるモデル)

//mesh type enumeration
enum class DEFAULT_MESH
{
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
inline Model GetDefaultModel(DEFAULT_MESH type)
{
	//メッシュタイプに応じたメッシュデータを返す
	switch (type)
	{
	case DEFAULT_MESH::QUAD: return MakeQuadModel();			//四角平面
	case DEFAULT_MESH::CUBE: return MakeCubeModel();			//立方体
	case DEFAULT_MESH::SPHERE: return MakeSphereModel();		//球体
	case DEFAULT_MESH::CIRCLE: return MakeCircleModel();		//円形平面
	case DEFAULT_MESH::CAPSULE: return MakeCapsuleModel();	//カプセル
	case DEFAULT_MESH::CYLINDER: return MakeCylinderModel();	//円柱
	default:   return {};						//その他
	}
}