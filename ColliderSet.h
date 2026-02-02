#pragma once
#include "Collider.h"
#include "SharedStruct.h"
#include <vector>
#include <DirectXMath.h>

class CollisionManager;

class ColliderSet
{
public:
	ColliderSet(
		Actor* owner,
		OBJECT_TAG ownerTag,
		DirectX::XMFLOAT3 basePosition,
		DirectX::XMFLOAT3 baseScale,
		DirectX::XMFLOAT3 baseRotation,
		CollisionData::COLLISION_LAYER layer,
		bool enabled = true,
		DirectX::XMFLOAT3 offsetPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3 offsetRotation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		bool isTrigger = false
	);
	~ColliderSet();

	void Update();
	void RegisterColliders(CollisionManager& collisionManager); //コライダーをコリジョンマネージャーに提出
	void BuildObjectCollisionInfos();		//衝突情報を収集

	void AddCollider(	//コライダー追加
		ColliderType type,				//コライダータイプ
		DirectX::XMFLOAT3 localCenter,	//ローカル中心座標
		DirectX::XMFLOAT3 localScale,	//ローカルスケール
		DirectX::XMFLOAT3 localRotation	//ローカル回転
	);

	//ゲッター
	Actor* GetOwner() const { return m_owner; } //所有者オブジェクト取得
	const std::vector<Collider*>& GetColliders() const;	//コライダー配列取得
	std::vector<CollisionData::ObjectCollisionInfo>& GetCollisionInfos() { return m_collisionInfos; } //衝突情報配列取得
	const DirectX::XMFLOAT3& GetBasePosition() const { return m_basePosition; }	//基準位置取得
	const DirectX::XMFLOAT3& GetBaseScale() const { return m_baseScale; }		//基準スケール取得
	const DirectX::XMFLOAT3& GetBaseRotation() const { return m_baseRotation; }	//基準回転取得
	CollisionData::COLLISION_LAYER GetLayer() const { return m_layer; }	//衝突レイヤー取得
	const bool IsActive() const { return m_isActive; }	//有効フラグ取得

	//セッター
	void SetActive(bool flag);	//有効フラグ設定
	void SetDeleteFlag(bool flag);	//デリートフラグ設定
	void ClearCollisionInfos();		//衝突情報配列クリア
	void SetBaseScale(DirectX::XMFLOAT3 baseScale);	//基準スケール設定

private:
	Actor* m_owner;
	OBJECT_TAG m_ownerTag; //所有者オブジェクトのタグ
	std::vector<Collider*> m_colliders;
	std::vector<CollisionData::ObjectCollisionInfo> m_collisionInfos; //衝突情報配列
	CollisionData::COLLISION_LAYER m_layer;
	bool m_isActive = true;
	bool m_isTrigger = false;

	DirectX::XMFLOAT3 m_basePosition{};	//基準位置
	DirectX::XMFLOAT3 m_baseScale{};	//基準スケール
	DirectX::XMFLOAT3 m_baseRotation{};	//基準回転

	DirectX::XMFLOAT3 m_offsetPosition{};	//オフセット位置
	DirectX::XMFLOAT3 m_offsetRotation{};	//オフセット回転

	DirectX::XMFLOAT3 m_ownerBaseScale{};		//所有者オブジェクトの基準スケール
	DirectX::XMFLOAT3 m_baseScaleOffset{};
};