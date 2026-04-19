#pragma once
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>
#include <vector>
#include <string>
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine/Graphics/VertexBuffer.h"
#include "Engine/Graphics/IndexBuffer.h"
#include "Engine/Core/Math/Math.h"

// Vertex data structure
struct Vertex
{
	Vector3 position;	//vertex position
	Vector3 normal;	//vertex normal
	Vector2 uv;		//vertex UV coordinates
	Vector3 tangent;	//vertex tangent
	Vector4 color;	//vertex color

	static const D3D12_INPUT_LAYOUT_DESC InputLayout; //input layout

private:
	static const size_t InputLayoutCount = 5;								//input layout element count
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputLayoutCount];	//input element array
};

// Skinned vertex data structure
//struct SkinnedVertex
//{
//	DirectX::XMFLOAT3 position;		//vertex position
//	DirectX::XMFLOAT3 normal;		//vertex normal
//	DirectX::XMFLOAT2 uv;			//vertex UV coordinates
//	DirectX::XMFLOAT3 tangent;		//vertex tangent
//	DirectX::XMFLOAT4 color;		//vertex color
//
//	DirectX::XMINT4 boneIndices;	//bone indices
//	DirectX::XMFLOAT4 boneWeights;	//bone weights
//
//	static const D3D12_INPUT_LAYOUT_DESC InputLayout; //input layout
//};

//UV rectangle structure
struct UVRect
{
	float u = 0.0f;		//UV rectangle X coordinate
	float v = 0.0f;		//UV rectangle Y coordinate
	float su = 1.0f;	//UV rectangle width
	float sv = 1.0f;	//UV rectangle height
};

// Frame constant buffer structure for camera (b0)
struct alignas(256) FrameConstants
{
	// Camera related data
	Matrix4x4 view;	//view matrix
	Matrix4x4 proj;	//projection matrix
};

// Object constant buffer structure for mesh rendering (b1)
struct alignas(256) MeshRenderConstants
{
	Matrix4x4 worldMatrix;			//world matrix
	Matrix4x4 worldInvTranspose;	//world inverse transpose matrix
	Vector4 objectColor;			//object color (RGBA)
};

// Object constant buffer structure for sprite rendering (b1)
struct alignas(256) SpriteRenderConstants
{
	Matrix4x4 worldMatrix;			//world matrix
	Vector4 color;	//sprite color (RGBA)
	Vector4 uvRect;	//UV rectangle
	Vector2 pivot;	//pivot point for the sprite
	Vector2 flip;	//flip flags for X and Y axes (1 for normal, -1 for flipped)
};

// Light constant buffer structure for point lights (b2)
struct alignas(256) LightConstants
{
	//lighting related data
	Vector4 lightDir_Intensity;	//light direction (xyz) and intensity (w)
	Vector4 lightColor_Ambient;	//light color (xyz) and ambient intensity (w)
};

//directional light structure
struct DirectionalLight
{
	Vector3 direction = { -1.0f, -1.0f, 0.0f };	//light direction
	float intensity = 1.0f;						//light intensity
	Vector3 color = { 1.0f, 1.0f, 1.0f };		//light color
	float ambient = 0.1f;						//ambient light intensity
	bool enabled = true;						//light enabled flag
};

//enum class for object tags
enum class ACTOR_TAG
{
	NONE = 0,		//none
	PLAYER,			//player
	ENEMY,			//enemy
	WALL,			//wall	
	PLAYER_BULLET,	//bullet
	ENEMY_BULLET,	//enemy bullet
	MAX				//max
};

//シーン列挙体
enum class SCENE_TYPE
{
	SCENE_NONE = 0,		//シーン無し
	SCENE_TITLE,		//タイトルシーン
	SCENE_GAME,			//ゲームシーン
	SCENE_RESULT,		//リザルトシーン
};

//Sprite splitting information structure
struct TexSplitInfo
{
	int index = 0;			//Sprite index
	int cols = 1;			//Number of columns
	int rows = 1;			//Number of rows
	int total = 1;			//Total number of frames (columns * rows)
	int frameCount = 0;		//Current frame count
	int updateRate = 0;		//Update rate (how many frames to advance)

	float offsetU = 0.0f;	//UV offset U
	float offsetV = 0.0f;	//UV offset V

	float scaleU = 1.0f;	//UV scale U
	float scaleV = 1.0f;	//UV scale V
};

//Get UV rectangle from sprite splitting information
inline static Vector4 SplitSprite(TexSplitInfo info)
{
	const float baseSu = 1.0f / static_cast<float>(info.cols);	//Basic UV scale U
	const float baseSv = 1.0f / static_cast<float>(info.rows);	//Basic UV scale V

	const int col = info.index % info.cols;	//Current column
	const int row = info.index / info.cols;	//Current row

	const float frameU = static_cast<float>(col) * baseSu;	//Frame UV offset U
	const float frameV = static_cast<float>(row) * baseSv;	//Frame UV offset V

	const float minU = frameU + info.offsetU * baseSu;	//Minimum U coordinate
	const float minV = frameV + info.offsetV * baseSv;	//Minimum V coordinate

	const float sizeU = baseSu * info.scaleU;	//Maximum U coordinate
	const float sizeV = baseSv * info.scaleV;	//Maximum V coordinate

	//UV rectangle creation
	return Vector4(minU, minV, sizeU, sizeV);
}