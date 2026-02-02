#pragma once
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>
#include <DirectXMath.h>
#include "DirectXTex.h"
#include <vector>
#include <string>
#include "ComPtr.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "TextureManager.h"

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

//transform structure for 3D objects
struct Transform3D
{
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };	//position
	DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };		//scale
	DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };	//rotation
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

//camera information structure
struct CameraInfo
{
	DirectX::XMFLOAT3 position;	//camera position
	DirectX::XMFLOAT3 target;	//camera target point
	DirectX::XMFLOAT3 up;		//camera up direction vector
	DirectX::XMFLOAT3 right;	//camera right direction vector
	float fov;					//vertical field of view
	float aspectRatio;			//aspect ratio
	float nearZ;				//near clip distance
	float farZ;					//far clip distance
};

//enum class for object tags
enum class OBJECT_TAG
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

//前方宣言
class Collider;
class ObjectBase;

//衝突データ用名前空間
namespace CollisionData
{
	//enumlaration for collision states
	enum COLLISION_STATE
	{
		COLLISION_NONE = 0,	//No collision
		COLLISION_ENTER,	//Collision start
		COLLISION_STAY,		//Collision stay
		COLLISION_EXIT,		//Collision exit
	};

	//enumeration for collision layers
	enum class COLLISION_LAYER
	{
		DEFAULT = 0,	//default
		PLAYER,			//player
		ENEMY,			//enemy
		WALL,			//wall
		PLAYER_BULLET,	//player bullet
		PLAYER_RAY,		//player raycast
		ENEMY_BULLET,	//enemy bullet
		MAX_LAYER		//max layer
	};

	//Layer mask type definition
	using LayerMask = uint32_t;

	//Collider collision information structure
	struct CollisionInfo
	{
		Collider* opponent = nullptr;								//opponent collider
		DirectX::XMFLOAT3 contactPoint = { 0.0f, 0.0f, 0.0f };		//collision point
		DirectX::XMFLOAT3 contactNormal = { 0.0f, 0.0f, 0.0f };		//collision normal
		DirectX::XMFLOAT3 penetrationDepth = { 0.0f, 0.0f, 0.0f };	//penetration depth
		CollisionData::COLLISION_STATE state =
			CollisionData::COLLISION_STATE::COLLISION_NONE;			//collision state
	};

	//Object collision information structure
	struct ObjectCollisionInfo
	{
		ObjectBase* opponent = nullptr;								//opponent object
		DirectX::XMFLOAT3 contactPoint = { 0.0f, 0.0f, 0.0f };		//collision point
		DirectX::XMFLOAT3 contactNormal = { 0.0f, 0.0f, 0.0f };		//collision normal
		DirectX::XMFLOAT3 penetrationDepth = { 0.0f, 0.0f, 0.0f };	//penetration depth
		CollisionData::COLLISION_STATE state =
			CollisionData::COLLISION_STATE::COLLISION_NONE;			//collision state
	};

	//Raycast hit information structure
	struct RaycastHitInfo
	{
		Collider* opponent = nullptr;						//opponent collider
		DirectX::XMFLOAT3 hitPoint = { 0.0f, 0.0f, 0.0f };	//collision point
		DirectX::XMFLOAT3 hitNormal = { 0.0f, 0.0f, 0.0f };	//collision normal
		float hitDistance = 0.0f;							//collision distance
		COLLISION_STATE state =
			COLLISION_STATE::COLLISION_NONE;				//collision state
	};

	//Raycast segment structure
	struct RaycastSegment
	{
		DirectX::XMFLOAT3 startPoint = { 0.0f, 0.0f, 0.0f };	//start point
		DirectX::XMFLOAT3 endPoint = { 0.0f, 0.0f, 0.0f };		//end point
		LayerMask layerMask = 0;								//layer mask
		COLLISION_LAYER layer = COLLISION_LAYER::DEFAULT;		//layer
		std::vector<RaycastHitInfo> currHitInfos;					//hit information array
		std::vector<RaycastHitInfo> prevHitInfos;				//previous hit information array
	};

	//Layer to bit conversion function
	LayerMask LayerToBit(COLLISION_LAYER layer);

	//Layer mask retrieval function
	LayerMask GetLayerMask(COLLISION_LAYER layer);

	//Layer mask creation function from initializer list
	LayerMask MakeMask(std::initializer_list<COLLISION_LAYER> layers);

	//Get push-out vector from penetration depth
	DirectX::XMFLOAT3 GetPushOutVector(
		std::vector<CollisionData::ObjectCollisionInfo>& infos,	//collision information array
		const std::initializer_list<OBJECT_TAG>& tagList		//target tag list
	);
}


//=======================
//Vector operation functions
//=======================

// Add two vectors
inline static DirectX::XMFLOAT3 AddXMF3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
	return DirectX::XMFLOAT3{
		a.x + b.x,
		a.y + b.y,
		a.z + b.z
	};
}

// Subtract two vectors
inline static DirectX::XMFLOAT3 SubtractXMF3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
	return DirectX::XMFLOAT3{
		a.x - b.x,
		a.y - b.y,
		a.z - b.z
	};
}

// Multiply XMFLOAT4 by XMFLOAT4
inline static DirectX::XMFLOAT4 MultiplyXMF4(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b)
{
	return DirectX::XMFLOAT4{
		a.x * b.x,
		a.y * b.y,
		a.z * b.z,
		a.w * b.w
	};
}

//Calculate the square of the distance between two points
inline static float LengthSqBetween(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
	DirectX::XMFLOAT3 diff{
		b.x - a.x,
		b.y - a.y,
		b.z - a.z
	};
	return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
}

//Calculate the distance between two points
inline static float LengthBetween(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
	return sqrtf(LengthSqBetween(a, b));
}

//Calculate the length of a vector (XMFLOAT3 version)
inline static float LengthXMF3(const DirectX::XMFLOAT3& v)
{
	return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

//Calculate the length of a vector (XMVECTOR version)
inline static float LengthXMV(const DirectX::XMVECTOR& v)
{
	DirectX::XMFLOAT3 temp;
	DirectX::XMStoreFloat3(&temp, v);
	return sqrtf(temp.x * temp.x + temp.y * temp.y + temp.z * temp.z);
}

//Normalize a vector
inline static DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& v)
{
	float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	if (len > 0.0f)
	{
		return DirectX::XMFLOAT3{ v.x / len, v.y / len, v.z / len };
	}
	else
	{
		return DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	}
}

//Calculate the dot product of two vectors
inline static float Dot(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

//Calculate the cross product of two vectors
inline static DirectX::XMFLOAT3 Cross(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
	return DirectX::XMFLOAT3{
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

//Clamp a value between 0.0 and 1.0
inline static float Clamp01(float value)
{
	if (value < 0.0f)
	{
		return 0.0f;
	}
	else if (value > 1.0f)
	{
		return 1.0f;
	}
	else
	{
		return value;
	}
}

//Calculate linear interpolation (float version)
inline static float Lerpf(float start, float end, float t)
{
	return start + (end - start) * t;
}

//Calculate linear interpolation (XMFLOAT3 version)
inline static DirectX::XMFLOAT3 LerpXMF3(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, float t)
{
	return DirectX::XMFLOAT3{
		start.x + (end.x - start.x) * t,
		start.y + (end.y - start.y) * t,
		start.z + (end.z - start.z) * t
	};
}

//Calculate linear interpolation (XMVECTOR version)
inline static DirectX::XMVECTOR LerpXMV(const DirectX::XMVECTOR& start, const DirectX::XMVECTOR& end, float t)
{
	return DirectX::XMVectorLerp(start, end, t);
}

//quadratic easing in
inline static float EaseInQuad(float t)
{
	return t * t;
}

//cubic easing in
inline static float EaseInCubic(float t)
{
	return t * t * t;
}

//back easing in
inline static float EaseOutQuad(float t)
{
	return 1.0f - (1.0f - t) * (1.0f - t);
}

//cubic easing out
inline static float EaseOutCubic(float t)
{
	float p = 1.0f - t;
	return 1.0f - p * p * p;
}

//back easing out
inline static float EaseOutBack(float t)
{
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;
	float u = t - 1.0f;
	return 1.0f + c3 * u * u * u + c1 * u * u;
}

//3D transformation information combination function
inline static Transform3D CombineTransform3D(const Transform3D& world, const Transform3D& local)
{
	Transform3D result;
	//Scale combination
	result.scale.x = world.scale.x * local.scale.x;
	result.scale.y = world.scale.y * local.scale.y;
	result.scale.z = world.scale.z * local.scale.z;

	//Rotation combination
	result.rotation.x = world.rotation.x + local.rotation.x;
	result.rotation.y = world.rotation.y + local.rotation.y;
	result.rotation.z = world.rotation.z + local.rotation.z;

	//Position combination (taking scale and rotation into account)
	DirectX::XMVECTOR childPos = DirectX::XMLoadFloat3(&local.position);	//Child position vector
	DirectX::XMVECTOR parentScale = DirectX::XMLoadFloat3(&world.scale);	//Parent scale vector
	childPos = DirectX::XMVectorMultiply(childPos, parentScale);			//Apply scale

	DirectX::XMVECTOR parentRot = DirectX::XMLoadFloat3(&world.rotation);	//Parent rotation vector
	
	const float pitch = DirectX::XMConvertToRadians(DirectX::XMVectorGetX(parentRot));
	const float yaw = DirectX::XMConvertToRadians(DirectX::XMVectorGetY(parentRot));
	const float roll = DirectX::XMConvertToRadians(DirectX::XMVectorGetZ(parentRot));
	
	DirectX::XMMATRIX rotMatrix =											//Parent rotation matrix
		DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
	childPos = DirectX::XMVector3Transform(childPos, rotMatrix);				//Apply rotation

	DirectX::XMVECTOR parentPos = DirectX::XMLoadFloat3(&world.position);		//Parent position vector
	DirectX::XMVECTOR resultPos = DirectX::XMVectorAdd(parentPos, childPos);	//Position combination
	DirectX::XMStoreFloat3(&result.position, resultPos);						//Store result

	return result;
}

//Get transformation matrix from transformation information
inline static DirectX::XMMATRIX GetMatrixFromTransform3D(const Transform3D& transform)
{
	//Scale matrix
	DirectX::XMMATRIX scaleMatrix =
		DirectX::XMMatrixScaling(
			transform.scale.x,
			transform.scale.y,
			transform.scale.z
		);
	//Rotation matrix
	DirectX::XMMATRIX rotMatrix =
		DirectX::XMMatrixRotationRollPitchYaw(
			DirectX::XMConvertToRadians(transform.rotation.x),	//Pitch
			DirectX::XMConvertToRadians(transform.rotation.y),	//Yaw
			DirectX::XMConvertToRadians(transform.rotation.z)	//Roll
		);
	//Translation matrix
	DirectX::XMMATRIX transMatrix =
		DirectX::XMMatrixTranslation(
			transform.position.x,
			transform.position.y,
			transform.position.z
		);
	//World matrix composition (Scale → Rotation → Translation)
	return scaleMatrix * rotMatrix * transMatrix;
}

//3D transformation matrix combination function
inline static DirectX::XMMATRIX CombineMatrix3D(const Transform3D& world, const Transform3D& local)
{
	return DirectX::XMMatrixMultiply(GetMatrixFromTransform3D(local),GetMatrixFromTransform3D(world));
}

//Get transformation matrix from position, scale, and rotation
inline static DirectX::XMMATRIX GetMatrixFromGeometry(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& scale,
	const DirectX::XMFLOAT3& rotation
)
{
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(
		DirectX::XMConvertToRadians(rotation.x),
		DirectX::XMConvertToRadians(rotation.y),
		DirectX::XMConvertToRadians(rotation.z));
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
	return S * R * T;
}

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