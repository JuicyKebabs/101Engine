#include "Collider.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Core/Debug/Debug.h"

using namespace DirectX;

void Collider::Initialize(const InitDesc& desc)
{
	m_localTransform = { desc.localCenter, desc.localRotation, desc.localScale };
	m_type = desc.type;
	m_layer = desc.layer;
	m_isTrigger = desc.isTrigger;
	m_layerMask = MakeLayerMask(m_layer);
}

void Collider::OnStart()
{
	m_layerMask = MakeLayerMask(m_layer);

	auto owner = GetOwner();
	if (owner)
	{
		m_ownerTag = owner->GetTag();
		auto ownerScene = owner->GetOwner();
		if (ownerScene)
		{
			ownerScene->GetCollisionSystem()->Register(this);
		}
		else
		{
			DBG("Collider : Owner scene is null.");
		}
	}
	else
	{
		DBG("Collider : Owner actor is null.");
	}
}

void Collider::PreUpdate(float deltaTime)
{
}

//コライダー変換更新
void Collider::Update(float deltaTime)
{
}

void Collider::LateUpdate(float deltaTime)
{
	ChackIfTransformChanged();

	if (!m_isDirty) return;

	auto owner = GetOwner();
	if (!owner)
	{
		DBG("Collider : Owner actor is null.");
		return;
	}
	auto ownerTransform = owner->GetTransform();
	if (!ownerTransform)
	{
		DBG("Collider : Owner transform form is null.");
		return;
	}

	m_worldTransformCurrent.position = Vector3::Transform(m_localTransform.position, ownerTransform->GetWorldMatrix());

	m_worldTransformCurrent.rotation = ownerTransform->GetWorldRotationQuat() * m_localTransform.rotation;

	UpdateCollider(ownerTransform->GetWorldScale());

	UpdateAABB();
}

void Collider::OnDestroy()
{
	m_isActive = false;
	m_deleteFlag = true;
}

//コライダー更新
void Collider::UpdateCollider(Vector3 ownerScale)
{
	//各種コライダー更新
	switch (m_type)
	{
	case ColliderType::None:
		return;
		break;
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
	m_worldTransformPrevious = m_worldTransformCurrent;
}

//コライダータイプ取得
ColliderType Collider::GetType() const
{
	return m_type;
}

//衝突レイヤー取得
CollisionLayer Collider::GetLayer() const
{
	return m_layer;
}

//衝突レイヤーマスク取得
LayerMask Collider::GetLayerMask() const
{
	return m_layerMask;
}

//トリガーフラグ取得
bool Collider::IsTrigger() const
{
	return m_isTrigger;
}

//SWEPT軸平行境界ボックス取得
const AABB& Collider::GetSewptAABB() const
{
	return m_sweptAABB;
}

//現在のボックスコライダー取得
const BoxCollider& Collider::GetCurrentBoxCollider() const
{
	return m_currentBoxCollider;
}

//前回のボックスコライダー取得
const BoxCollider& Collider::GetPreviousBoxCollider() const
{
	return m_previousBoxCollider;
}

//現在の球コライダー取得
const SphereCollider& Collider::GetCurrentSphereCollider() const
{
	return m_currentSphereCollider;
}

//前回の球コライダー取得
const SphereCollider& Collider::GetPreviousSphereCollider() const
{
	return m_previousSphereCollider;
}

//現在のカプセルコライダー取得
const CapsuleCollider& Collider::GetCurrentCapsuleCollider() const
{
	return m_currentCapsuleCollider;
}

//前回のカプセルコライダー取得
const CapsuleCollider& Collider::GetPreviousCapsuleCollider() const
{
	return m_previousCapsuleCollider;
}

//ワールド行列の取得
Matrix4x4 Collider::GetWorldMatrix() const
{
	return Matrix4x4::CreateTRS(m_worldTransformCurrent.position, m_worldTransformCurrent.rotation, m_worldTransformCurrent.scale);
}

//所有者オブジェクトのタグ取得
ACTOR_TAG Collider::GetOwnerTag() const
{
	return m_ownerTag;
}

//衝突情報配列取得
std::vector<CollisionInfo>& Collider::GetCollisionInfos()
{
	return m_collisionInfos;
}

//衝突検知フラグ取得
bool Collider::isDetected() const
{
	return m_isDetected;
}

//デリートフラグ
bool Collider::deleteFlag() const
{
	return m_deleteFlag;
}

//アクティブフラグ取得
bool Collider::isActive() const
{
	return m_isActive;
}

const Transform3D& Collider::GetWorldTransformCurrent() const
{
	return m_worldTransformCurrent;
}

const Transform3D& Collider::GetWorldTransformPrevious() const
{
	return m_worldTransformPrevious;
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
	m_isDirty = true;
}

void Collider::ChackIfTransformChanged()
{
	auto owner = GetOwner();
	if (owner) {
		auto transform = owner->GetTransform();
		if (transform) {
			uint64_t currentGeneration = transform->GetWorldGeneration();
			if (m_transformGeneration != currentGeneration) {
				m_transformGeneration = currentGeneration;
				m_isDirty = true;
			}
		}
	}
}

//コライダー更新関数
void Collider::UpdateBoxCollider(Vector3 ownerScale)
{
	m_currentBoxCollider.center = m_worldTransformCurrent.position;	//ボックスコライダー中心点更新

	//スケール反映
	m_currentBoxCollider.scale =
	{
		ownerScale.x * m_localTransform.scale.x,
		ownerScale.y * m_localTransform.scale.y,
		ownerScale.z * m_localTransform.scale.z
	};

	//コライダーサイズ更新
	m_worldTransformCurrent.scale = m_currentBoxCollider.scale;
}

//球コライダー更新
void Collider::UpdateSphereCollider(Vector3 ownerScale)
{
	m_currentSphereCollider.center = m_worldTransformCurrent.position;	//球コライダー中心点更新

	//スケール反映
	XMFLOAT3 scale =
	{
		ownerScale.x * m_localTransform.scale.x,
		ownerScale.y * m_localTransform.scale.y,
		ownerScale.z * m_localTransform.scale.z
	};


	const float diamiter = (std::max)(scale.x, (std::max)(scale.y, scale.z));	//直径(水平方向の最大値)
	const float radius = diamiter / 2.0f;										//半径

	m_worldTransformCurrent.scale = { diamiter, diamiter, diamiter };

	//球コライダーサイズ更新
	m_currentSphereCollider.radius = radius;				//球コライダー半径更新
}

//カプセルコライダー更新
void Collider::UpdateCapsuleCollider(Vector3 ownerScale)
{
	//スケール反映
	const XMFLOAT3 scale = 
	{
		ownerScale.x * m_localTransform.scale.x,
		ownerScale.y * m_localTransform.scale.y,
		ownerScale.z * m_localTransform.scale.z
	};
	const float diamiter = (std::max)(scale.x, scale.z);				//直径(水平方向の最大値)
	const float radius = diamiter / 2.0f;								//半径
	const float capusleHeight = scale.y;								//カプセル高さ
	const float cylHeight = (std::max)(0.0f, capusleHeight - diamiter);	//円柱部分の高さ(負の値にならないようにする)

	//カプセルサイズ更新
	m_currentCapsuleCollider.radius = radius;					//カプセルコライダー半径更新
	m_currentCapsuleCollider.cylHeight = cylHeight;				//カプセルコライダー高さ更新
	float halfHeight = cylHeight * 0.5f;					//シリンダーの半分の高さ

	Vector3 rotatedAxis = m_worldTransformCurrent.rotation.RotateVector3(Vector3::Up()).Normalized();
	Vector3 pA = m_worldTransformCurrent.position + rotatedAxis * halfHeight;
	Vector3 pB = m_worldTransformCurrent.position - rotatedAxis * halfHeight;

	m_currentCapsuleCollider.pointA = pA;
	m_currentCapsuleCollider.pointB = pB;

	//コライダーサイズ更新
	m_worldTransformCurrent.scale ={ diamiter, capusleHeight, diamiter};
}

//軸平行境界ボックス更新(ボックスコライダー用)
void Collider::UpdateAABBBox()
{
	//ボックスコライダー情報取得
	const Vector3 center = m_worldTransformCurrent.position;
	const Vector3 scale = m_worldTransformCurrent.scale;
	const float halfX = scale.x * 0.5f;
	const float halfY = scale.y * 0.5f;
	const float halfZ = scale.z * 0.5f;

	Matrix4x4 R = Matrix4x4::CreateFromQuaternion(m_worldTransformCurrent.rotation);
	Vector4 xAxis = R.GetCol(0);
	Vector4 yAxis = R.GetCol(1);
	Vector4 zAxis = R.GetCol(2);

	//AABB半分のサイズ計算
	float aabbHalfX =
		fabsf(xAxis.x) * halfX +
		fabsf(yAxis.x) * halfY +
		fabsf(zAxis.x) * halfZ;
	float aabbHalfY =
		fabsf(xAxis.y) * halfX +
		fabsf(yAxis.y) * halfY +
		fabsf(zAxis.y) * halfZ;
	float aabbHalfZ =
		fabsf(xAxis.z) * halfX +
		fabsf(yAxis.z) * halfY +
		fabsf(zAxis.z) * halfZ;

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
	const Vector3 center = m_worldTransformCurrent.position;

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
	const Vector3 pointA = m_currentCapsuleCollider.pointA;
	const Vector3 pointB = m_currentCapsuleCollider.pointB;
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
