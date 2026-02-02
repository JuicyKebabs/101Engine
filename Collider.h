#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "SharedStruct.h"

//前方宣言
class Actor;
class ColliderSet;

//コライダータイプ列挙型
enum class ColliderType
{
	BOX,		//ボックス
	SPHERE,		//球
	CAPSULE,	//カプセル
	NONE		//なし
};

//ボックスコライダー構造体
struct BoxCollider
{
	DirectX::XMFLOAT3 center;		//中心点
	DirectX::XMFLOAT3 scale;		//サイズ（幅、高さ、奥行き）
	DirectX::XMFLOAT3 defaultScale;	//初期サイズ
};

//球コライダー構造体
struct SphereCollider
{
	DirectX::XMFLOAT3 center;	//中心点
	float radius;				//半径
	float defaultRadius;		//初期半径
};

//カプセルコライダー構造体
struct CapsuleCollider
{
	DirectX::XMFLOAT3 pointA;	//端点A(底面中心)
	DirectX::XMFLOAT3 pointB;	//端点B(頂点中心)
	float cylHeight;				//端点間の高さ
	float defaultHeight;		//初期高さ
	float radius;				//半径
	float defaultRadius;		//初期半径
};

//軸平行境界ボックス構造体
struct AABB
{
	DirectX::XMFLOAT3 min;	//最小座標
	DirectX::XMFLOAT3 max;	//最大座標
};

//衝突判定用の矩形クラス
class Collider
{
public:
	Collider(		//コンストラクタ
		ColliderSet* parentSet,					//親コライダーセットポインタ
		DirectX::XMFLOAT3 localCenter,				//ローカル中心座標
		DirectX::XMFLOAT3 localScale,				//ローカルスケール
		DirectX::XMFLOAT3 localRotation,			//ローカル回転
		ColliderType type,							//コライダータイプ
		OBJECT_TAG ownerTag,						//所有者オブジェクトのタグ
		CollisionData::COLLISION_LAYER layer =
		CollisionData::COLLISION_LAYER::DEFAULT,	//衝突レイヤー
		bool isTrigger = false						//トリガーフラグ
	);
	~Collider();	//デストラクタ

	//コライダー更新関数
	void Update(
		DirectX::XMFLOAT3 ownerPosition,	//オブジェクト位置
		DirectX::XMFLOAT3 ownerScale,	//オブジェクトスケール
		DirectX::XMFLOAT3 ownerRotation	//オブジェクト回転
	);
	void UpdateCollider(DirectX::XMFLOAT3 ownerScale);		//各種コライダー更新
	void UpdateAABB();			//AABB更新
	void SetPreviousState();	//前回状態の保存

	//衝突情報操作関数
	void AddCollisionInfo(const CollisionData::CollisionInfo& info);	//衝突情報追加
	void ClearInfos();													//衝突情報配列クリア

	//ゲッター
	ColliderSet* GetParentSet() const;								//親コライダーセットポインタ取得
	ColliderType GetType() const;									//コライダータイプ取得
	CollisionData::COLLISION_LAYER GetLayer() const;				//衝突レイヤー取得
	CollisionData::LayerMask GetLayerMask() const;					//衝突レイヤーマスク取得
	const bool IsTrigger() const;									//トリガーフラグ取得
	const AABB GetSewptAABB();										//SWEPT軸平行境界ボックス取得
	const BoxCollider GetCurrentBoxCollider();						//現在のボックスコライダー取得
	const BoxCollider GetPreviousBoxCollider();						//前回のボックスコライダー取得
	const SphereCollider GetCurrentSphereCollider();				//現在の球コライダー取得
	const SphereCollider GetPreviousSphereCollider();				//前回の球コライダー取得
	const CapsuleCollider GetCurrentCapsuleCollider();				//現在のカプセルコライダー取得
	const CapsuleCollider GetPreviousCapsuleCollider();				//前回のカプセルコライダー取得
	const DirectX::XMMATRIX GetWorldMatrix() const;					//ワールド行列の取得
	const OBJECT_TAG& GetOwnerTag() const;							//所有者オブジェクトのタグ取得
	std::vector<CollisionData::CollisionInfo>& GetCollisionInfos();	//衝突情報配列取得
	const bool isDetected() const;									//衝突検知フラグ取得
	const bool deleteFlag() const;									//デリートフラグ
	const bool isActive() const;									//アクティブフラグ取得
	DirectX::XMFLOAT3 GetCurrentCenter() const;						//現在の中心座標取得
	DirectX::XMFLOAT3 GetPreviousCenter() const;					//前回の中心座標取得
	DirectX::XMFLOAT3 GetCurrentScale() const;						//現在のサイズ取得
	DirectX::XMFLOAT3 GetPreviousScale() const;						//前回のサイズ取得
	DirectX::XMFLOAT3 GetRotation() const;							//回転取得

	//セッター
	void SetDetected(bool flag);	//衝突検知フラグ
	void SetDeleteFlag(bool flag);	//デリートフラグ
	void SetActive(bool flag);		//アクティブフラグ設定

private:
	ColliderSet* m_parentSet;				//親コライダーセットポインタ
	ColliderType m_type;					//コライダータイプ
	CollisionData::COLLISION_LAYER m_layer;	//衝突レイヤー
	CollisionData::LayerMask m_layerMask;	//衝突レイヤーマスク
	bool m_isTrigger = false;				//トリガーフラグ(物理衝突を無視するかどうか)

	//軸平行境界ボックス
	AABB m_currentAABB;		//現在の軸平行境界ボックス(BroadPhase用)
	AABB m_previousAABB;	//前回の軸平行境界ボックス(BroadPhase用)
	AABB m_sweptAABB;		//SweptAABB(BroadPhase用)

	//各種コライダー(テスト用に全て保持)
	BoxCollider m_currentBoxCollider;			//現在のボックスコライダー
	BoxCollider m_previousBoxCollider;			//前回のボックスコライダー
	SphereCollider m_currentSphereCollider;		//現在の球コライダー
	SphereCollider m_previousSphereCollider;	//前回の球コライダー
	CapsuleCollider m_currentCapsuleCollider;	//現在のカプセルコライダー
	CapsuleCollider m_previousCapsuleCollider;	//前回のカプセルコライダー

	//ローカル情報
	DirectX::XMFLOAT3 m_localCenter;	//ローカル中心座標
	DirectX::XMFLOAT3 m_localScale;		//ローカルサイズ
	DirectX::XMFLOAT3 m_localRotation;	//ローカル回転

	//ワールド情報
	DirectX::XMFLOAT3 m_currentCenter;	//現在の中心座標
	DirectX::XMFLOAT3 m_previousCenter;	//前回の中心座標
	DirectX::XMFLOAT3 m_currentScale;	//現在のサイズ
	DirectX::XMFLOAT3 m_previousScale;	//前回のサイズ
	DirectX::XMFLOAT3 m_rotation;		//回転
	DirectX::XMFLOAT3 m_scaleOffset;	//オブジェクトとのサイズ差

	OBJECT_TAG m_ownerTag;		//所有者オブジェクトのタグ
	std::vector<CollisionData::CollisionInfo> m_collisionInfos; //衝突情報配列(所有者オブジェクト用)

	bool m_isDetected = false;	//衝突検知フラグ（描画用）
	bool m_deleteFlag = false;	//デリートフラグ
	bool m_isActive = true;		//アクティブフラグ

private:
	//コライダー更新関数
	void UpdateBoxCollider(DirectX::XMFLOAT3 ownerScale);		//ボックスコライダー更新
	void UpdateSphereCollider(DirectX::XMFLOAT3 ownerScale);	//球コライダー更新
	void UpdateCapsuleCollider(DirectX::XMFLOAT3 ownerScale);	//カプセルコライダー更新

	//AABB更新関数
	void UpdateAABBBox();			//AABB更新(ボックスコライダー用)
	void UpdateAABBSphere();		//AABB更新(球コライダー用)
	void UpdateAABBCapsule();		//AABB更新(カプセルコライダー用)
	void MakeSweptAABB();			//SweptAABB作成
};