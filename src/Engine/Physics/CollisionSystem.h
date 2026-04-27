#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include "Engine/Component/Collider.h"
#include "Engine/Core/Utility/SharedStruct.h"
#include "Engine/Graphics/RenderData.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/Physics/CollisionData.h"

// Structure to define a pair of colliders for collision checking
struct CollisionPair
{
	Collider* colliderA; // Collider A
	Collider* colliderB; // Collider B
};

// Structure for Oriented Bounding Box (OBB)
struct OBB
{
	DirectX::XMVECTOR center;		// Center point
	DirectX::XMVECTOR axis[3];		// Direction vectors for each axis (normalized)
	Vector3 halfSizes;	// Half sizes along each axis
};

// Structure for sphere segment (used for raycasting)
struct SphereSegment
{
	DirectX::XMVECTOR center;	// Center point
	float radius;				// Radius
};

// Structure for capsule segment
struct CapsuleSegment
{
	DirectX::XMVECTOR pointA;	// Edge point A (bottom center)
	DirectX::XMVECTOR pointB;	// Edge point B (top center)
	float radius;				// Radius
};

// Structure for contact result of a collision
struct ContactResult
{
	bool isCollided = false;	// Collision detected flag
	DirectX::XMVECTOR point{};	// Contact point
	DirectX::XMVECTOR normal{};	// Contact normal vector
	float depth = 0.0f;			// Penetration depth
};

// Collision system class responsible for managing colliders and performing collision detection
class CollisionSystem
{
public:
	CollisionSystem() = default;	// Constructor
	~CollisionSystem() = default;	// Destructor

	void Register(Collider* collider);		// Register a collider to the collision system
	void Unregister(Collider* collider);	// Unregister a collider from the collision system
	void ClearColliders();					// Clear all colliders from the collision system
	void CheckColliders();					// Check each collider if it is active and perform collision detection

	void CheckCollisions();							// Check collisions between registered colliders
	void CheckCollisionStates();					// Check collision states
	void RaycastSegmentQuery(RaycastSegment& ray);	// Perform raycast segment query for collision detection

private:
	std::vector<Collider*> m_pCollidersList;				// Pointer array of colliders registered in the collision system
	std::vector<CollisionPair> m_pNarrowPhaseColliders;		// Array of collider pairs for narrow phase collision detection
	std::vector<CollisionPair> m_currentCollisionPairs;		// Current collision pairs array (pairs that are currently colliding)
	std::vector<CollisionPair> m_previousCollisionPairs;	// Previous collision pairs array (pairs that were colliding in the previous frame)

private:
	//ChackCollisions()の補助関数
	void BroadPhase();	//ブロードフェーズ
	void NarrowPhase();	//ナローフェーズ

	ContactResult NarrowPhaseCollision(	//ナローフェーズの衝突判定
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);

	bool CheckLayer(	//衝突レイヤーのチェック
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);

	void RegisterCollisionPair(	//衝突ペアを登録
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);

	void UpdateCollisionState();	//前回の衝突ペアと比較して新規衝突か継続衝突かをチェック

	//各種衝突判定関数
	bool CollisionAABB(	//ボックス対ボックスの衝突判定
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);
	ContactResult CollisionBoxToBox(	//ボックス対ボックスの衝突判定
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);
	ContactResult CollisionSphereToSphere(	//球対球の衝突判定
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);
	ContactResult CollisionCapsuleToCapsule(	//カプセル対カプセルの衝突判定
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);
	ContactResult CollisionBoxToSphere(	//ボックス対球の衝突判定
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);
	ContactResult CollisionBoxToCapsule(	//ボックス対カプセルの衝突判定
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);
	ContactResult CollisionSphereToCapsule(	//球対カプセルの衝突判定
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);

	//継続的衝突検出(CCD)用関数
	ContactResult CollisionBoxToCapsuleCCD(	//ボックス対カプセルの衝突判定(継続的衝突検出)
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);

	//補助関数
	ContactResult CollisonOBBtoCapsule(	//OBB対カプセルの衝突判定
		const OBB& obb,					//OBB
		const CapsuleSegment& capsule	//カプセルセグメント
	);
	ContactResult CollisionSpheresSegments(	//球セグメント同士の衝突判定
		const SphereSegment& sphereA,	//球セグメントA
		const SphereSegment& sphereB	//球セグメントB
	);

	void SendNarrowPhase( //ナローフェーズ用配列に衝突ペアを追加
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);

	OBB CreateOBB(	//コライダーからOBBを作成(LERP補間付き)
		Collider* collider,	//コライダー
		float alpfa = 1.0f	//LERP補間係数
	);

	CapsuleSegment CreateCapsuleSegment(	//コライダーからカプセルセグメントを作成(LERP補間付き)
		Collider* collider,	//コライダー
		float alpfa = 1.0f	//LERP補間係数
	);

	bool NeedsCCD(Collider* collider);	//継続的衝突検出が必要かどうかチェック

	int CalculateSubsteps(	//継続的衝突検出のサブステップ数を計算
		Collider* colliderA,	//コライダーA
		Collider* colliderB		//コライダーB
	);

	static float GetMinDistanceSquaredSegmentToSegment(	//セグメント間の最小距離の二乗を取得
		const DirectX::FXMVECTOR& p0,
		const DirectX::FXMVECTOR& p1,
		const DirectX::FXMVECTOR& q0,
		const DirectX::FXMVECTOR& q1,
		DirectX::XMVECTOR& outP,
		DirectX::XMVECTOR& outQ
	);
	static float GetMinDistanceSquaredPointToSegment(	//点とセグメント間の最小距離の二乗を取得
		const DirectX::FXMVECTOR& point,	//点
		const DirectX::FXMVECTOR& segA,		//セグメントの端点A
		const DirectX::FXMVECTOR& segB,		//セグメントの端点B
		DirectX::XMVECTOR& outClosest		//セグメント上の最短点
	);
	static float GetMinDistanceSquaredPointToOBB(	//点とOBB間の最小距離の二乗を取得
		const DirectX::FXMVECTOR& point,	//点
		const OBB& obb,					//OBB
		DirectX::XMVECTOR& outClosest	//OBB上の最短点
	);
	static bool PairExistsinList(	//衝突ペアが保存されているかどうかチェック
		const CollisionPair& pair,							//衝突ペア
		const std::vector<CollisionPair>& collisionPairs	//衝突ペア配列
	);
	static void SetCollisionState(	//コライダーの衝突ステートを設定
		Collider* self,							//自分自身のコライダー
		Collider* opponent,						//衝突相手のコライダー
		COLLISION_STATE state	//衝突状態
	);
	static void PushCollisionInfo(	//コライダーに衝突情報を追加
		Collider* colliderA,	//自分自身のコライダー
		Collider* colliderB,	//衝突相手のコライダー
		ContactResult& result	//衝突時のパラメータ
	);
	static void OrientNormalAToB(	//法線ベクトルをAからBの方向に向ける
		Collider* colliderA,	//自分自身のコライダー
		Collider* colliderB,	//衝突相手のコライダー
		ContactResult& result	//衝突時のパラメータ
	);

	//レイキャスト補助関数
	bool CheckLayerRaycast(	//レイキャスト用衝突レイヤーのチェック
		const RaycastSegment& ray,	//レイ情報
		Collider* collider					//コライダー
	);
	void UpdateRaycastCollisionState(	//レイキャストの衝突状態チェック
		RaycastSegment& ray	//レイ情報
	);

	bool RaycastBox(	//AABBコライダーへのレイキャスト
		const RaycastSegment& ray,	//レイ情報
		Collider* collider,						//コライダー
		RaycastHitInfo& outHitInfo	//ヒット情報
	);
	bool RaycastSphere(	//球コライダーへのレイキャスト
		const RaycastSegment& ray,	//レイ情報
		Collider* collider,						//コライダー
		RaycastHitInfo& outHitInfo	//ヒット情報
	);
	bool RaycastCapsule(	//カプセルコライダーへのレイキャスト
		const RaycastSegment& ray,	//レイ情報
		Collider* collider,						//コライダー
		RaycastHitInfo& outHitInfo	//ヒット情報
	);
};