#include "Collider.h"
#include "ColliderSet.h"
#include "Actor.h"

using namespace DirectX;


//コンストラクタ
Collider::Collider(
	ColliderSet* parentSet,				//親コライダーセットポインタ
	DirectX::XMFLOAT3 localCenter,		//ローカル中心座標
	DirectX::XMFLOAT3 localScale,		//ローカルスケール
	DirectX::XMFLOAT3 localRotation,	//ローカル回転
	ColliderType type,					//コライダータイプ
	OBJECT_TAG ownerTag,				//所有者オブジェクトのタグ
	COLLISION_LAYER layer,				//衝突レイヤー
	bool isTrigger						//トリガーフラグ
) :
	m_parentSet(parentSet),
	m_localCenter(localCenter),
	m_localScale(localScale),
	m_localRotation(localRotation),
	m_type(type),
	m_layer(layer),
	m_ownerTag(ownerTag),
	m_isTrigger(isTrigger)
{

	m_layerMask = MakeLayerMask(m_layer); //衝突レイヤーマスク取得
}

//デストラクタ
Collider::~Collider()
{
}

//コライダー変換更新
void Collider::Update(
	DirectX::XMFLOAT3 ownerPosition, 
	DirectX::XMFLOAT3 ownerScale, 
	DirectX::XMFLOAT3 ownerRotation)
{
	XMMATRIX ownerRotationMatrix = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(ownerRotation.x),
		XMConvertToRadians(ownerRotation.y),
		XMConvertToRadians(ownerRotation.z));

	XMVECTOR localCenterVec = XMLoadFloat3(&m_localCenter);
	XMVECTOR ownerScaleVec = XMLoadFloat3(&ownerScale);
	XMVECTOR scaledLocal = XMVectorMultiply(localCenterVec, ownerScaleVec);

	XMVECTOR rotated = XMVector3Transform(scaledLocal, ownerRotationMatrix);

	XMVECTOR ownerPositionVec = XMLoadFloat3(&ownerPosition);
	XMVECTOR worldCenterVec = XMVectorAdd(rotated, ownerPositionVec);
	XMStoreFloat3(&m_currentCenter, worldCenterVec);

	m_rotation = 
	{
		ownerRotation.x + m_localRotation.x,
		ownerRotation.y + m_localRotation.y,
		ownerRotation.z + m_localRotation.z
	};

	UpdateCollider(ownerScale);	//各種コライダー更新

	UpdateAABB();
}

//コライダー更新
void Collider::UpdateCollider(DirectX::XMFLOAT3 ownerScale)
{
	//各種コライダー更新
	switch (m_type)
	{
	case ColliderType::BOX:
		UpdateBoxCollider(ownerScale);
		break;
	case ColliderType::SPHERE:
		UpdateSphereCollider(ownerScale);
		break;
	case ColliderType::CAPSULE:
		UpdateCapsuleCollider(ownerScale);
		break;
	default:
		break;
	}
}

//軸平行境界ボックス更新
void Collider::UpdateAABB()
{
	//現在のAABB更新
	switch (m_type)
	{
	case ColliderType::BOX:
		UpdateAABBBox();
		break;
	case ColliderType::SPHERE:
		UpdateAABBSphere();
		break;
	case ColliderType::CAPSULE:
		UpdateAABBCapsule();
		break;
	default:
		break;
	}

	//SweptAABB計算
	MakeSweptAABB();
}

//前回の状態保存
void Collider::SetPreviousState()
{
	m_previousBoxCollider = m_currentBoxCollider;			//前回のボックスコライダー保存
	m_previousSphereCollider = m_currentSphereCollider;		//前回の球コライダー保存
	m_previousCapsuleCollider = m_currentCapsuleCollider;	//前回のカプセルコライダー保存
	m_previousAABB = m_currentAABB;							//前回のAABB保存
	m_previousCenter = m_currentCenter;						//前回の中心座標保存
	m_previousScale = m_currentScale;						//前回のサイズ保存
}

//親コライダーセットポインタ取得
ColliderSet* Collider::GetParentSet() const
{
	return m_parentSet;
}

//コライダータイプ取得
ColliderType Collider::GetType() const
{
	return m_type;
}

//衝突レイヤー取得
COLLISION_LAYER Collider::GetLayer() const
{
	return m_layer;
}

//衝突レイヤーマスク取得
LayerMask Collider::GetLayerMask() const
{
	return m_layerMask;
}

//トリガーフラグ取得
const bool Collider::IsTrigger() const
{
	return m_isTrigger;
}

//SWEPT軸平行境界ボックス取得
const AABB Collider::GetSewptAABB()
{
	return m_sweptAABB;
}

//現在のボックスコライダー取得
const BoxCollider Collider::GetCurrentBoxCollider()
{
	return m_currentBoxCollider;
}

//前回のボックスコライダー取得
const BoxCollider Collider::GetPreviousBoxCollider()
{
	return m_previousBoxCollider;
}

//現在の球コライダー取得
const SphereCollider Collider::GetCurrentSphereCollider()
{
	return m_currentSphereCollider;
}

//前回の球コライダー取得
const SphereCollider Collider::GetPreviousSphereCollider()
{
	return m_previousSphereCollider;
}

//現在のカプセルコライダー取得
const CapsuleCollider Collider::GetCurrentCapsuleCollider()
{
	return m_currentCapsuleCollider;
}

//前回のカプセルコライダー取得
const CapsuleCollider Collider::GetPreviousCapsuleCollider()
{
	return m_previousCapsuleCollider;
}

//ワールド行列の取得
const DirectX::XMMATRIX Collider::GetWorldMatrix() const
{
	XMMATRIX T = XMMatrixTranslation(m_currentCenter.x, m_currentCenter.y, m_currentCenter.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(m_rotation.x),
		XMConvertToRadians(m_rotation.y),
		XMConvertToRadians(m_rotation.z));
	XMMATRIX S = XMMatrixScaling(m_currentScale.x, m_currentScale.y, m_currentScale.z);
	return S * R * T;
}

//所有者オブジェクトのタグ取得
const OBJECT_TAG& Collider::GetOwnerTag() const
{
	return m_ownerTag;
}

//衝突情報配列取得
std::vector<CollisionInfo>& Collider::GetCollisionInfos()
{
	return m_collisionInfos;
}

//衝突検知フラグ取得
const bool Collider::isDetected() const
{
	return m_isDetected;
}

//デリートフラグ
const bool Collider::deleteFlag() const
{
	return m_deleteFlag;
}

//アクティブフラグ取得
const bool Collider::isActive() const
{
	return m_isActive;
}

//中心座標取得
DirectX::XMFLOAT3 Collider::GetCurrentCenter() const
{
	return m_currentCenter;
}

//前回の中心座標取得
DirectX::XMFLOAT3 Collider::GetPreviousCenter() const
{
	return m_previousCenter;
}

//サイズ取得
DirectX::XMFLOAT3 Collider::GetCurrentScale() const
{
	return m_currentScale;
}

//前回のサイズ取得
DirectX::XMFLOAT3 Collider::GetPreviousScale() const
{
	return m_previousScale;
}

//回転取得
DirectX::XMFLOAT3 Collider::GetRotation() const
{
	return m_rotation;
}

//衝突検知フラグ設定
void Collider::SetDetected(bool flag)
{
	m_isDetected = flag;
}

//デリートフラグ設定
void Collider::SetDeleteFlag(bool flag)
{
	m_deleteFlag = flag;
}

//アクティブフラグ設定
void Collider::SetActive(bool flag)
{
	m_isActive = flag;
}

//コライダー更新関数
void Collider::UpdateBoxCollider(DirectX::XMFLOAT3 ownerScale)
{
	//スケール反映
	m_currentBoxCollider.scale =
	{
		ownerScale.x * m_localScale.x,
		ownerScale.y * m_localScale.y,
		ownerScale.z * m_localScale.z
	};

	//コライダーサイズ更新
	m_currentScale = m_currentBoxCollider.scale;

	m_currentBoxCollider.center = m_currentCenter;	//ボックスコライダー中心点更新
}

//球コライダー更新
void Collider::UpdateSphereCollider(DirectX::XMFLOAT3 ownerScale)
{
	//中心点更新
	m_currentSphereCollider.center = m_currentCenter;	//球コライダー中心点更新

	//スケール反映
	XMFLOAT3 scale =
	{
		ownerScale.x * m_localScale.x,
		ownerScale.y * m_localScale.y,
		ownerScale.z * m_localScale.z
	};


	const float diamiter =
		(std::max)(scale.x, (std::max)(scale.y, scale.z));	//直径(水平方向の最大値)
	const float radius = diamiter / 2.0f;					//半径

	m_currentScale = { diamiter, diamiter, diamiter };

	//球コライダーサイズ更新
	m_currentSphereCollider.radius = radius;				//球コライダー半径更新
}

//カプセルコライダー更新
void Collider::UpdateCapsuleCollider(DirectX::XMFLOAT3 ownerScale)
{
	//スケール反映
	const XMFLOAT3 scale = 
	{
		ownerScale.x * m_localScale.x,
		ownerScale.y * m_localScale.y,
		ownerScale.z * m_localScale.z
	};
	const float diamiter =
		(std::max)(scale.x, scale.z);		//直径(水平方向の最大値)
	const float radius = diamiter / 2.0f;	//半径
	const float capusleHeight = scale.y;	//カプセル高さ
	const float cylHeight =
		(std::max)(0.0f, capusleHeight - diamiter);				//円柱部分の高さ(負の値にならないようにする)

	//カプセルサイズ更新
	m_currentCapsuleCollider.radius = radius;					//カプセルコライダー半径更新
	m_currentCapsuleCollider.cylHeight = cylHeight;				//カプセルコライダー高さ更新

	//ローカル軸方向ベクトル取得
	XMVECTOR dirLocal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); //Y軸方向ベクトル

	//回転行列取得
	XMMATRIX R = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(m_rotation.x),
		XMConvertToRadians(m_rotation.y),
		XMConvertToRadians(m_rotation.z));

	//ワールド軸方向ベクトル計算
	XMVECTOR axisWorld = XMVector3Normalize(XMVector3TransformNormal(dirLocal, R));

	//端点A,B計算
	float halfHeight = cylHeight * 0.5f;					//シリンダーの半分の高さ
	XMVECTOR center = XMLoadFloat3(&m_currentCenter);				//中心点
	XMVECTOR offset = XMVectorScale(axisWorld, halfHeight);	//オフセットベクトル

	XMVECTOR pA = XMVectorAdd(center, offset);	//端点A
	XMVECTOR pB = XMVectorSubtract(center, offset);		//端点B

	XMStoreFloat3(&m_currentCapsuleCollider.pointA, pA);	//端点A保存
	XMStoreFloat3(&m_currentCapsuleCollider.pointB, pB);	//端点B保存

	//コライダーサイズ更新
	m_currentScale =
	{
		diamiter,
		capusleHeight,
		diamiter
	};
}

//軸平行境界ボックス更新(ボックスコライダー用)
void Collider::UpdateAABBBox()
{
	//ボックスコライダー情報取得
	const XMFLOAT3 center = m_currentCenter;
	const XMFLOAT3 scale = m_currentBoxCollider.scale;
	const float halfX = scale.x * 0.5f;
	const float halfY = scale.y * 0.5f;
	const float halfZ = scale.z * 0.5f;

	//回転行列取得
	XMMATRIX R = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(m_rotation.x),
		XMConvertToRadians(m_rotation.y),
		XMConvertToRadians(m_rotation.z));

	//各軸方向ベクトル取得
	XMVECTOR u0 = XMVector3Normalize(R.r[0]); //X軸方向ベクトル
	XMVECTOR u1 = XMVector3Normalize(R.r[1]); //Y軸方向ベクトル
	XMVECTOR u2 = XMVector3Normalize(R.r[2]); //Z軸方向ベクトル

	//AABB半分のサイズ計算
	float aabbHalfX =
		fabsf(XMVectorGetX(u0)) * halfX +
		fabsf(XMVectorGetX(u1)) * halfY +
		fabsf(XMVectorGetX(u2)) * halfZ;
	float aabbHalfY =
		fabsf(XMVectorGetY(u0)) * halfX +
		fabsf(XMVectorGetY(u1)) * halfY +
		fabsf(XMVectorGetY(u2)) * halfZ;
	float aabbHalfZ =
		fabsf(XMVectorGetZ(u0)) * halfX +
		fabsf(XMVectorGetZ(u1)) * halfY +
		fabsf(XMVectorGetZ(u2)) * halfZ;

	//AABB更新
	m_currentAABB.min = {
		center.x - aabbHalfX,
		center.y - aabbHalfY,
		center.z - aabbHalfZ
	};
	m_currentAABB.max = {
		center.x + aabbHalfX,
		center.y + aabbHalfY,
		center.z + aabbHalfZ
	};
}

//軸平行境界ボックス更新(球コライダー用)
void Collider::UpdateAABBSphere()
{
	const float radius = m_currentSphereCollider.radius;
	const XMFLOAT3 center = m_currentCenter;

	m_currentAABB.min = {
	center.x - radius,
	center.y - radius,
	center.z - radius
	};

	m_currentAABB.max = {
	center.x + radius,
	center.y + radius,
	center.z + radius
	};
}

//軸平行境界ボックス更新(カプセルコライダー用)
void Collider::UpdateAABBCapsule()
{
	const float radius = m_currentCapsuleCollider.radius;
	const XMFLOAT3 pointA = m_currentCapsuleCollider.pointA;
	const XMFLOAT3 pointB = m_currentCapsuleCollider.pointB;
	m_currentAABB.min = {
		(std::min)(pointA.x, pointB.x) - radius,
		(std::min)(pointA.y, pointB.y) - radius,
		(std::min)(pointA.z, pointB.z) - radius
	};
	m_currentAABB.max = {
		(std::max)(pointA.x, pointB.x) + radius,
		(std::max)(pointA.y, pointB.y) + radius,
		(std::max)(pointA.z, pointB.z) + radius
	};
}

//SweptAABB作成関数
void Collider::MakeSweptAABB()
{
	AABB s{};
	s.min.x = (std::min)(m_previousAABB.min.x, m_currentAABB.min.x);
	s.min.y = (std::min)(m_previousAABB.min.y, m_currentAABB.min.y);
	s.min.z = (std::min)(m_previousAABB.min.z, m_currentAABB.min.z);

	s.max.x = (std::max)(m_previousAABB.max.x, m_currentAABB.max.x);
	s.max.y = (std::max)(m_previousAABB.max.y, m_currentAABB.max.y);
	s.max.z = (std::max)(m_previousAABB.max.z, m_currentAABB.max.z);

	m_sweptAABB = s;
}
