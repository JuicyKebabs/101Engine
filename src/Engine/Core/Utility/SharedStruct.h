#pragma once
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>
#include <DirectXMath.h>
#include "DirectXTex.h"
#include <vector>
#include <string>
#include "Engine/Core/ComPtr/ComPtr.h"
#include "Engine/Graphics/VertexBuffer.h"
#include "Engine/Graphics/IndexBuffer.h"

// Vertex data structure
struct Vertex
{
	DirectX::XMFLOAT3 position;	//vertex position
	DirectX::XMFLOAT3 normal;	//vertex normal
	DirectX::XMFLOAT2 uv;		//vertex UV coordinates
	DirectX::XMFLOAT3 tangent;	//vertex tangent
	DirectX::XMFLOAT4 color;	//vertex color

	static const D3D12_INPUT_LAYOUT_DESC InputLayout; //input layout

private:
	static const size_t InputLayoutCount = 5;								//input layout element count
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputLayoutCount];	//input element array
};

// Skinned vertex data structure
struct SkinnedVertex
{
	DirectX::XMFLOAT3 position;		//vertex position
	DirectX::XMFLOAT3 normal;		//vertex normal
	DirectX::XMFLOAT2 uv;			//vertex UV coordinates
	DirectX::XMFLOAT3 tangent;		//vertex tangent
	DirectX::XMFLOAT4 color;		//vertex color

	DirectX::XMINT4 boneIndices;	//bone indices
	DirectX::XMFLOAT4 boneWeights;	//bone weights

	static const D3D12_INPUT_LAYOUT_DESC InputLayout; //input layout
};

//UV rectangle structure
struct UVRect
{
	float u = 0.0f;		//UV rectangle X coordinate
	float v = 0.0f;		//UV rectangle Y coordinate
	float su = 1.0f;	//UV rectangle width
	float sv = 1.0f;	//UV rectangle height
};

//constant buffer structure for transformations
struct alignas(256) PerObjectConstants
{
	//transformation matrices
	DirectX::XMMATRIX worldMatrix;			//world matrix
	DirectX::XMMATRIX worldInvTranspose;	//world inverse transpose matrix
	DirectX::XMMATRIX viewMatrix;			//view matrix
	DirectX::XMMATRIX projMatrix;			//projection matrix

	//image related data
	DirectX::XMFLOAT4 objectColor;	//object color (RGBA)
	DirectX::XMFLOAT4 uvRect;		//UV rectangle

	//lighting related data
	DirectX::XMFLOAT4 lightDir_Intensity;	//light direction (xyz) and intensity (w)
	DirectX::XMFLOAT4 lightColor_Ambient;	//light color (xyz) and ambient intensity (w)
};

//directional light structure
struct DirectionalLight
{
	DirectX::XMFLOAT3 direction = { -1.0f, -1.0f, 0.0f };	//light direction
	float intensity = 1.0f;									//light intensity
	DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };			//light color
	float ambient = 0.1f;									//ambient light intensity
	bool enabled = true;									//light enabled flag
};

//constant buffer structure for effects
struct EffectCB
{
	//camera related data
	DirectX::XMMATRIX viewProj;
	DirectX::XMFLOAT3 camRight;
	float _pad0;
	DirectX::XMFLOAT3 camUp;
	float _pad1;

	//effect related data
	DirectX::XMFLOAT3 center;
	float _pad2;
	DirectX::XMFLOAT2 size;
	DirectX::XMFLOAT2 _padSize;
	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT4 uvRect;
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

//シーンコンテキスト構造体
struct SceneContext
{
};

//=======================
//Vector operation functions
//=======================

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
inline static DirectX::XMFLOAT4 SplitSprite(TexSplitInfo info)
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
	return DirectX::XMFLOAT4(minU, minV, sizeU, sizeV);
}

//Convert quaternion to Euler angles
inline DirectX::XMFLOAT3 QuaternionToEuler(const DirectX::XMVECTOR& q)
{
	using namespace DirectX;

	// Convert XMVECTOR quaternion to XMFLOAT4
	XMFLOAT4 fq;
	XMStoreFloat4(&fq, q);

	//X axis rotation (Pitch)
	float sinr_cosp = 2.0f * (fq.w * fq.x + fq.y * fq.z);
	float cosr_cosp = 1.0f - 2.0f * (fq.x * fq.x + fq.y * fq.y);
	float rotX = std::atan2(sinr_cosp, cosr_cosp);

	//Y axis rotation (Yaw)
	float sinp = 2.0f * (fq.w * fq.y - fq.z * fq.x);
	if (sinp > 1.0f)  sinp = 1.0f;
	if (sinp < -1.0f) sinp = -1.0f;
	float rotY = std::asin(sinp);

	//Z axis rotation (Roll)
	float siny_cosp = 2.0f * (fq.w * fq.z + fq.x * fq.y);
	float cosy_cosp = 1.0f - 2.0f * (fq.y * fq.y + fq.z * fq.z);
	float rotZ = std::atan2(siny_cosp, cosy_cosp);

	//Convert radians to degrees
	return XMFLOAT3(rotX, rotY, rotZ);
}

// Calculate look rotation (Euler angles) from direction vector
inline static DirectX::XMFLOAT3 CalcLookRotationFromDir(const DirectX::XMFLOAT3& dir)
{
	using namespace DirectX;

	XMVECTOR direction = XMLoadFloat3(&dir);
	float lengthSq = XMVectorGetX(XMVector3LengthSq(direction));
	if (lengthSq < 1e-6f)
	{
		return XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
	direction = XMVector3Normalize(direction);

	XMFLOAT3 angle;
	XMStoreFloat3(&angle, direction);

	float yawRad = std::atan2f(angle.x, angle.z);
	float xzLen = std::sqrtf(angle.x * angle.x + angle.z * angle.z);
	float pitchRad = std::atan2f(-angle.y, xzLen);
	float rollRad = 0.0f;

	return XMFLOAT3(
		XMConvertToDegrees(pitchRad),
		XMConvertToDegrees(yawRad),
		XMConvertToDegrees(rollRad)
	);
}