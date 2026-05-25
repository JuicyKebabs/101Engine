#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "Engine/Component/Component.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Physics/CollisionData.h"

// Forward declaration
class Actor;

// Enumration for collider types
enum class ColliderType
{
	BOX,		// Box
	SPHERE,		// Sphere
	CAPSULE,	// Capsule
	None		// None
};

// Structure for box collider
struct BoxCollider
{
	Vector3 center;		// Center point
	Vector3 scale;		// Size
	Vector3 defaultScale;	// Initial size
};

// Structure for sphere collider
struct SphereCollider
{
	Vector3 center;	// Center point
	float radius;				// Radius
	float defaultRadius;		// Initial radius
};

// Structure for capsule collider
struct CapsuleCollider
{
	Vector3 pointA;	// Endpoint A (bottom center)
	Vector3 pointB;	// Endpoint B (top center)
	float cylHeight;			// Height between endpoints
	float defaultHeight;		// Initial height
	float radius;				// Radius
	float defaultRadius;		// Initial radius
};

// Structure for axis-aligned bounding box (AABB)
struct AABB
{
	Vector3 min;	// Minimum point
	Vector3 max;	// Maximum point
};

class Collider : public Component
{
public:
	struct ParamDesc
	{
		Vector3 localCenter = Vector3(0, 0, 0);				// Local center position
		Quaternion localRotation = Quaternion::Identity();	// Local rotation
		Vector3 localScale = Vector3(1, 1, 1);				// Local scale
		ColliderType type = ColliderType::None;				// Collider type
		CollisionLayer layer = CollisionLayer::Default;		// Collision layer
		bool isTrigger = false;								// Trigger flag (whether to ignore physical collisions)
		std::string name = "Collider";						// Component name (optional, can be used for debugging or identification)
	};

public:
	Collider() = default;
	~Collider() = default;
	void SetParams(const ParamDesc& desc) {
		m_localTransform = { desc.localCenter, desc.localRotation, desc.localScale };
		m_type = desc.type;
		m_layer = desc.layer;
		m_isTrigger = desc.isTrigger;
		m_layerMask = MakeLayerMask(m_layer);
		SetName(desc.name);
	}

	void OnStartOverride() override;
	void PreUpdateOverride(float deltaTime) override;
	void UpdateOverride(float deltaTime) override;
	void LateUpdateOverride(float deltaTime) override;
	void OnDestroyOverride() override;
	void Flush();

	void AddCollisionInfo(const CollisionInfo& info) { m_collisionInfos.push_back(info); };	//衝突情報追加
	void ClearCollisionInfos() { m_collisionInfos.clear(); }								//衝突情報配列クリア

	void UpdateCollider(Vector3 ownerScale);		//各種コライダー更新
	void UpdateAABB();			//AABB更新
	void SetPreviousState();	//前回状態の保存

	//ゲッター
	const std::vector<CollisionInfo>& GetCollisionInfos() const { return m_collisionInfos; }	//衝突情報配列取得
	ColliderType GetType() const;									//コライダータイプ取得
	CollisionLayer GetLayer() const;				//衝突レイヤー取得
	LayerMask GetLayerMask() const;					//衝突レイヤーマスク取得
	bool IsTrigger() const;									//トリガーフラグ取得
	const AABB& GetSewptAABB() const;										//SWEPT軸平行境界ボックス取得
	const BoxCollider& GetCurrentBoxCollider() const;						//現在のボックスコライダー取得
	const BoxCollider& GetPreviousBoxCollider() const;						//前回のボックスコライダー取得
	const SphereCollider& GetCurrentSphereCollider() const;				//現在の球コライダー取得
	const SphereCollider& GetPreviousSphereCollider() const;				//前回の球コライダー取得
	const CapsuleCollider& GetCurrentCapsuleCollider() const;				//現在のカプセルコライダー取得
	const CapsuleCollider& GetPreviousCapsuleCollider() const;				//前回のカプセルコライダー取得
	Matrix4x4 GetWorldMatrix() const;					//ワールド行列の取得
	TagId GetOwnerTag() const;							//所有者オブジェクトのタグ取得
	std::vector<CollisionInfo>& GetCollisionInfos();	//衝突情報配列取得
	bool isDetected() const;									//衝突検知フラグ取得
	bool deleteFlag() const;									//デリートフラグ
	bool isActive() const;									//アクティブフラグ取得
	const Transform3D& GetWorldTransformCurrent() const;
	const Transform3D& GetWorldTransformPrevious() const;

	//セッター
	void SetDetected(bool flag);	//衝突検知フラグ
	void SetDeleteFlag(bool flag);	//デリートフラグ
	void SetActive(bool flag);		//アクティブフラグ設定

private:
	std::vector<CollisionInfo> m_collisionInfos;

	ColliderType m_type = ColliderType::None;			//コライダータイプ
	CollisionLayer m_layer = CollisionLayer::Default;	//衝突レイヤー
	LayerMask m_layerMask = 0;							//衝突レイヤーマスク
	bool m_isTrigger = false;							//トリガーフラグ(物理衝突を無視するかどうか)

	//軸平行境界ボックス
	AABB m_currentAABB;		//現在の軸平行境界ボックス(BroadPhase用)
	AABB m_previousAABB;	//前回の軸平行境界ボックス(BroadPhase用)
	AABB m_sweptAABB;		//SweptAABB(BroadPhase用)

	BoxCollider m_currentBoxCollider;			//現在のボックスコライダー
	BoxCollider m_previousBoxCollider;			//前回のボックスコライダー
	SphereCollider m_currentSphereCollider;		//現在の球コライダー
	SphereCollider m_previousSphereCollider;	//前回の球コライダー
	CapsuleCollider m_currentCapsuleCollider;	//現在のカプセルコライダー
	CapsuleCollider m_previousCapsuleCollider;	//前回のカプセルコライダー

	Transform3D m_localTransform{};
	Transform3D m_worldTransformCurrent{};
	Transform3D m_worldTransformPrevious{};

	Vector3 m_scaleOffset;	//オブジェクトとのサイズ差

	TagId m_ownerTag = TAG_NONE;	//所有者オブジェクトのタグ

	bool m_isActive = true;
	bool m_isDetected = false;	//衝突検知フラグ（描画用）
	bool m_deleteFlag = false;	//デリートフラグ

	bool m_isDirty = true;
	uint64_t m_transformGeneration = 0;

private:
	void ChackIfTransformChanged();
	void RefreshWorldTransform();

	//コライダー更新関数
	void UpdateBoxCollider(Vector3 ownerScale);		//ボックスコライダー更新
	void UpdateSphereCollider(Vector3 ownerScale);	//球コライダー更新
	void UpdateCapsuleCollider(Vector3 ownerScale);	//カプセルコライダー更新

	//AABB更新関数
	void UpdateAABBBox();			//AABB更新(ボックスコライダー用)
	void UpdateAABBSphere();		//AABB更新(球コライダー用)
	void UpdateAABBCapsule();		//AABB更新(カプセルコライダー用)
	void MakeSweptAABB();			//SweptAABB作成
};