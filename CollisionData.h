#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include <vector>
#include "SharedStruct.h"

// Forward declarations
class Collider;
class Actor;

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
	COLLISION_STATE state =
		COLLISION_STATE::COLLISION_NONE;			//collision state
};

//Object collision information structure
struct ObjectCollisionInfo
{
	Actor* opponent = nullptr;								//opponent object
	DirectX::XMFLOAT3 contactPoint = { 0.0f, 0.0f, 0.0f };		//collision point
	DirectX::XMFLOAT3 contactNormal = { 0.0f, 0.0f, 0.0f };		//collision normal
	DirectX::XMFLOAT3 penetrationDepth = { 0.0f, 0.0f, 0.0f };	//penetration depth
	COLLISION_STATE state =
		COLLISION_STATE::COLLISION_NONE;			//collision state
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
LayerMask MakeLayerMask(COLLISION_LAYER layer);

//Layer mask creation function from initializer list
LayerMask MakeMask(std::initializer_list<COLLISION_LAYER> layers);

//Get push-out vector from penetration depth
DirectX::XMFLOAT3 GetPushOutVector(
	std::vector<ObjectCollisionInfo>& infos,	//collision information array
	const std::initializer_list<OBJECT_TAG>& tagList		//target tag list
);
