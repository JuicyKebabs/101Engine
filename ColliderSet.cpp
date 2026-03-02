#include "ColliderSet.h"
#include "CollisionManager.h"
#include "Actor.h"

using namespace DirectX;


//コンストラクタ
ColliderSet::ColliderSet(
	Actor* m_owner, 
	OBJECT_TAG ownerTag,
	DirectX::XMFLOAT3 basePosition, DirectX::XMFLOAT3 baseScale, DirectX::XMFLOAT3 baseRotation,
	COLLISION_LAYER layer,
	bool enabled,
	DirectX::XMFLOAT3 offsetPosition, DirectX::XMFLOAT3 offsetRotation,
	bool isTrigger
	)
	: 
	m_owner(m_owner), 
	m_ownerTag(ownerTag),
	m_basePosition(basePosition), m_baseScale(baseScale), m_baseRotation(baseRotation),
	m_layer(layer),
	m_isActive(enabled),
	m_offsetPosition(offsetPosition), m_offsetRotation(offsetRotation),
	m_isTrigger(isTrigger)
{
	//m_ownerBaseScale = m_owner->GetScale();
	Update();
}

//デストラクタ
ColliderSet::~ColliderSet()
{
	for(auto& collider : m_colliders)
	{
		delete collider;
		collider = nullptr;
	}

	m_colliders.clear();
}

//更新
void ColliderSet::Update()
{
	//if (!m_isActive) return;

	////オーナーの変換情報取得
	//XMFLOAT3 ownerPosition = m_owner->GetPosition();
	//XMFLOAT3 ownerScale = m_owner->GetScale();
	//XMFLOAT3 ownerRotation = m_owner->GetRotation();

	////基準変換とオフセット変換を加算
	//m_basePosition.x = ownerPosition.x + m_offsetPosition.x;
	//m_basePosition.y = ownerPosition.y + m_offsetPosition.y;
	//m_basePosition.z = ownerPosition.z + m_offsetPosition.z;

	//auto safeDiv = [](float a, float b) { return (fabs(b) < 1e-6f) ? 1.0f : (a / b); };

	//XMFLOAT3 scaleRatio =
	//{
	//	safeDiv(ownerScale.x, m_ownerBaseScale.x),
	//	safeDiv(ownerScale.y, m_ownerBaseScale.y),
	//	safeDiv(ownerScale.z, m_ownerBaseScale.z)
	//};

	//XMFLOAT3 setScale =
	//{
	//	m_baseScale.x * scaleRatio.x,
	//	m_baseScale.y * scaleRatio.y,
	//	m_baseScale.z * scaleRatio.z,
	//};

	//m_baseRotation.x = ownerRotation.x + m_offsetRotation.x;
	//m_baseRotation.y = ownerRotation.y + m_offsetRotation.y;
	//m_baseRotation.z = ownerRotation.z + m_offsetRotation.z;

	////コライダーの変換更新
	//for(auto& collider : m_colliders)
	//{
	//	collider->Update(
	//		m_basePosition,
	//		setScale,
	//		m_baseRotation
	//	);
	//}
}

//コライダー提出
void ColliderSet::RegisterColliders(CollisionManager& collisionManager)
{
	if (!m_isActive) return;
	for (auto& collider : m_colliders)
	{
		collisionManager.RegisterCollider(collider);
	}
}

//コライダー追加
void ColliderSet::AddCollider(
	ColliderType type,							//コライダータイプ
	DirectX::XMFLOAT3 localCenter,				//ローカル中心座標
	DirectX::XMFLOAT3 localScale,				//ローカルスケール
	DirectX::XMFLOAT3 localRotation				//ローカル回転
)
{
	m_colliders.push_back(
		new Collider(
			this,
			localCenter,
			localScale,
			localRotation,
			type,
			m_ownerTag,
			m_layer,
			m_isTrigger
		)
	);
}

//コライダー配列取得
const std::vector<Collider*>& ColliderSet::GetColliders() const
{
	return m_colliders;
}

//衝突情報を収集
void ColliderSet::BuildObjectCollisionInfos()
{
	std::unordered_map<Actor*, ObjectCollisionInfo> collisionMap;	//オブジェクトごとの衝突情報を格納するマップ

	//各コライダーの衝突情報を収集
	for (auto& collider : m_colliders)
	{
		for(auto& info : collider->GetCollisionInfos())
		{
			//衝突相手のオブジェクト取得
			Collider* opponentCollider = info.opponent;
			if (!opponentCollider) continue; //相手コライダーがnullptrの場合スキップ

			ColliderSet* opponentSet = opponentCollider->GetParentSet();
			if (!opponentSet) continue; //相手コライダーセットがnullptrの場合スキップ

			Actor* opponentObject = opponentSet->GetOwner();
			if (!opponentObject) continue; //相手オブジェクトがnullptrの場合スキップ

			//オブジェクトごとの衝突情報をマップに格納
			auto& agg = collisionMap[opponentObject];

			//最初の衝突情報の場合、相手オブジェクトを設定
			if (!agg.opponent)
			{
				agg.opponent = opponentObject;
			}

			//貫入深さが最大の情報を保持
			if(LengthXMF3(info.penetrationDepth) > LengthXMF3(agg.penetrationDepth))
			{
				agg.contactPoint = info.contactPoint;
				agg.contactNormal = info.contactNormal;
				agg.penetrationDepth = info.penetrationDepth;
			}

			//衝突状態の優先度を決定するラムダ式
			auto priority = [](COLLISION_STATE state) {
					switch (state)
					{
					case COLLISION_STATE::COLLISION_STAY:
						return 3;
					case COLLISION_STATE::COLLISION_ENTER:
						return 2;
					case COLLISION_STATE::COLLISION_EXIT:
						return 1;
					default:
						return 0;
					}

					return 0;
				};

			//衝突状態の優先度が高いものを保持
			if (priority(info.state) > priority(agg.state))
			{
				agg.state = info.state;
			}
		}
	}

	//マップの内容を衝突情報配列に転送
	m_collisionInfos.clear();						//衝突情報配列クリア
	m_collisionInfos.reserve(collisionMap.size());	//必要な容量を確保
	for (auto& pair : collisionMap)					//マップ内の各要素を配列に追加
	{
		m_collisionInfos.push_back(pair.second);
	}
	collisionMap.clear();						//マップクリア
}

//有効フラグ設定
void ColliderSet::SetActive(bool enabled)
{
	m_isActive = enabled;

	for(auto& collider : m_colliders)
	{
		collider->SetActive(enabled);
	}
}

//デリートフラグ設定
void ColliderSet::SetDeleteFlag(bool flag)
{
	for (auto& collider : m_colliders)
	{
		collider->SetDeleteFlag(flag);
	}
}

//衝突情報配列クリア
void ColliderSet::ClearCollisionInfos()
{
	m_collisionInfos.clear();
	for(auto& collider : m_colliders)
	{
		collider->ClearInfos();
	}
}

//基準スケール設定
void ColliderSet::SetBaseScale(DirectX::XMFLOAT3 baseScale)
{
	m_baseScale = baseScale;
}
