#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include <vector>
#include "Engine/Core/Math/Math.h"
#include "Engine/Actor/ActorTag.h"

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
enum class CollisionLayer
{
	Default = 0,	//default
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
	Vector3 contactPoint = { 0.0f, 0.0f, 0.0f };		//collision point
	Vector3 contactNormal = { 0.0f, 0.0f, 0.0f };		//collision normal
	Vector3 penetrationDepth = { 0.0f, 0.0f, 0.0f };	//penetration depth
	COLLISION_STATE state =
		COLLISION_STATE::COLLISION_NONE;			//collision state
};

//Object collision information structure
struct ObjectCollisionInfo
{
	Actor* opponent = nullptr;								//opponent object
	Vector3 contactPoint = { 0.0f, 0.0f, 0.0f };		//collision point
	Vector3 contactNormal = { 0.0f, 0.0f, 0.0f };		//collision normal
	Vector3 penetrationDepth = { 0.0f, 0.0f, 0.0f };	//penetration depth
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
	CollisionLayer layer = CollisionLayer::Default;		//layer
	std::vector<RaycastHitInfo> currHitInfos;					//hit information array
	std::vector<RaycastHitInfo> prevHitInfos;				//previous hit information array
};

//Layer to bit conversion function
LayerMask LayerToBit(CollisionLayer layer);

//Layer mask retrieval function
LayerMask MakeLayerMask(CollisionLayer layer);

//Layer mask creation function from initializer list
LayerMask MakeMask(std::initializer_list<CollisionLayer> layers);

//Get push-out vector from penetration depth
DirectX::XMFLOAT3 GetPushOutVector(
	std::vector<ObjectCollisionInfo>& infos,	//collision information array
	const std::initializer_list<TagId>& tagList		//target tag list
);
