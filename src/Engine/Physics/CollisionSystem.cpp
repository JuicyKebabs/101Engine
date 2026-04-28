#include "CollisionSystem.h"
#include <algorithm>
#include <unordered_set>
#include <cfloat>
#include "Engine/Actor/Actor.h"
#include "Engine/Graphics/Renderer.h"
#include "Engine/Resource/TextureManager.h"
#include "Engine/Resource/MeshManager.h"
#include "Engine/Core/Utility/SharedStruct.h"

using namespace DirectX;

void CollisionSystem::Register(Collider* collider)
{
	if (std::find(m_pCollidersList.begin(), m_pCollidersList.end(), collider) == m_pCollidersList.end())
	{
		m_pCollidersList.push_back(collider);
	}
}

void CollisionSystem::Unregister(Collider* collider)
{
	m_pCollidersList.erase(std::remove(m_pCollidersList.begin(), m_pCollidersList.end(), collider), m_pCollidersList.end());
}

void CollisionSystem::CheckColliders()
{
	for (auto& c : m_pCollidersList)
	{
		if (c->deleteFlag() || !c->GetOwner()) { Unregister(c); }
		else if (!c->GetOwner()->IsActive()) { c->SetActive(false); }
	}

	//前回の衝突ペアリストを走査してデリートフラグが立っているコライダーを含むペアを削除
	for (auto it = m_previousCollisionPairs.begin(); it != m_previousCollisionPairs.end(); ) {
		if (it->colliderA->deleteFlag() || it->colliderB->deleteFlag()) { it = m_previousCollisionPairs.erase(it); }
		else { it++; }
	}
}

//衝突判定処理
void CollisionSystem::CheckCollisions()
{
	//各コライダーの初期化
	for (auto& collider : m_pCollidersList)
	{
		//各コライダーの衝突情報クリア
		collider->ClearCollisionInfos();
		//衝突検知フラグOFF
		collider->SetDetected(false);
	}

	//ブロードフェーズ
	BroadPhase();
	//ナローフェーズ
	NarrowPhase();

	//ナローフェーズ用配列クリア
	m_pNarrowPhaseColliders.clear();

	//衝突ステート更新
	UpdateCollisionState();

	//各コライダーの前回状態を保存
	for (auto collider : m_pCollidersList)
	{
		collider->SetPreviousState();
	}
}

//衝突状態チェック
void CollisionSystem::CheckCollisionStates()
{
	//衝突ステート更新
	UpdateCollisionState();
}

//ブロードフェーズ(衝突可能性のあるコライダーを絞り込む処理)
//AABB同士の簡易当たり判定
//当たっている可能性のあるコライダーをナローフェーズ用配列に追加
void CollisionSystem::BroadPhase()
{
	for (int i = 0; i < m_pCollidersList.size(); i++)
	{
		for (int j = i + 1; j < m_pCollidersList.size(); j++)
		{
			Collider* colliderA = m_pCollidersList[i];	//コライダーA
			Collider* colliderB = m_pCollidersList[j];	//コライダーB

			//active check
			if (!colliderA->isActive() || !colliderB->isActive())
			{
				continue;	//when not active, skip
			}

			//レイヤーチェック
			if (!CheckLayer(
				colliderA,	//コライダーA
				colliderB	//コライダーB
			))
			{
				continue;	//衝突しない場合はスキップ
			}

			//AABB同士の当たり判定
			bool isCollided = CollisionAABB(
				colliderA,	//コライダーA
				colliderB	//コライダーB
			);

			//衝突の可能性あり
			if (isCollided)
			{
				SendNarrowPhase(	//ナローフェーズ用配列に衝突ペアを追加
					colliderA,	//コライダーA
					colliderB	//コライダーB
				);
			}
		}
	}
}

//ナローフェーズ
void CollisionSystem::NarrowPhase()
{
	for (int i = 0; i < m_pNarrowPhaseColliders.size(); i++)
	{
		//コライダーの取得
		Collider* colliderA = m_pNarrowPhaseColliders[i].colliderA;
		Collider* colliderB = m_pNarrowPhaseColliders[i].colliderB;

		ContactResult result;	//衝突結果構造体

		//ナローフェーズの衝突判定
		result = NarrowPhaseCollision(colliderA, colliderB);

		//衝突していなければスキップ
		if (!result.isCollided) continue;

		//法線向きのチェック
		OrientNormalAToB(
			colliderA,	//コライダーA
			colliderB,	//コライダーB
			result		//衝突結果構造体
		);

		//衝突情報の登録
		PushCollisionInfo(
			colliderA,			//コライダーA
			colliderB,			//コライダーB
			result				//衝突結果構造体
		);

		//今回の衝突ペア配列に追加
		RegisterCollisionPair(colliderA, colliderB);
	}
}

//ナローフェーズの衝突判定
ContactResult CollisionSystem::NarrowPhaseCollision(Collider* colliderA, Collider* colliderB)
{
	//CCDの必要性チェック
	bool ccd = NeedsCCD(colliderA) || NeedsCCD(colliderB);

	//コライダータイプの取得
	ColliderType typeA = colliderA->GetType();
	ColliderType typeB = colliderB->GetType();

	//CCDが必要な場合
	if (ccd)
	{
		//ボックス対カプセルのみCCD対応
		if ((typeA == ColliderType::BOX && typeB == ColliderType::CAPSULE) ||
			(typeA == ColliderType::CAPSULE && typeB == ColliderType::BOX))
		{//ボックス対カプセル
			if (typeA == ColliderType::BOX)
			{
				return CollisionBoxToCapsuleCCD(colliderA, colliderB);
			}
			else
			{
				return CollisionBoxToCapsuleCCD(colliderB, colliderA);
			}
		}
	}

	//コライダータイプに応じた衝突判定関数の呼び出し
	if (typeA == ColliderType::BOX && typeB == ColliderType::BOX)
	{//ボックス対ボックス
		return CollisionBoxToBox(colliderA, colliderB);
	}
	else if (typeA == ColliderType::SPHERE && typeB == ColliderType::SPHERE)
	{//球対球
		return CollisionSphereToSphere(colliderA, colliderB);
	}
	else if (typeA == ColliderType::CAPSULE && typeB == ColliderType::CAPSULE)
	{//カプセル対カプセル
		return CollisionCapsuleToCapsule(colliderA, colliderB);
	}
	else if ((typeA == ColliderType::BOX && typeB == ColliderType::SPHERE) ||
		(typeA == ColliderType::SPHERE && typeB == ColliderType::BOX))
	{//ボックス対球
		if (typeA == ColliderType::BOX)
		{
			return CollisionBoxToSphere(colliderA, colliderB);
		}
		else
		{
			return CollisionBoxToSphere(colliderB, colliderA);
		}
	}
	else if ((typeA == ColliderType::BOX && typeB == ColliderType::CAPSULE) ||
		(typeA == ColliderType::CAPSULE && typeB == ColliderType::BOX))
	{//ボックス対カプセル
		if (typeA == ColliderType::BOX)
		{
			return CollisionBoxToCapsule(colliderA, colliderB);
		}
		else
		{
			return CollisionBoxToCapsule(colliderB, colliderA);
		}
	}
	else if ((typeA == ColliderType::SPHERE && typeB == ColliderType::CAPSULE) ||
		(typeA == ColliderType::CAPSULE && typeB == ColliderType::SPHERE))
	{//球対カプセル
		if (typeA == ColliderType::SPHERE)
		{
			return CollisionSphereToCapsule(colliderA, colliderB);
		}
		else
		{
			return CollisionSphereToCapsule(colliderB, colliderA);
		}
	}

	return ContactResult{}; //衝突しない場合
}

//レイヤーチェック
bool CollisionSystem::CheckLayer(
	Collider* colliderA, 
	Collider* colliderB
)
{
	LayerMask bitA = LayerToBit(colliderA->GetLayer());	//コライダーAのレイヤーマスク
	LayerMask bitB = LayerToBit(colliderB->GetLayer());	//コライダーBのレイヤーマスク

	LayerMask maskA = colliderA->GetLayerMask();	//コライダーAのレイヤーマスク
	LayerMask maskB = colliderB->GetLayerMask();	//コライダーBのレイヤーマスク

	bool aWantsB = (maskA & bitB) != 0;	//コライダーAがコライダーBと衝突したいかどうか
	bool bWantsA = (maskB & bitA) != 0;	//コライダーBがコライダーAと衝突したいかどうか

	return aWantsB && bWantsA;	//互いに衝突したい場合はtrueを返す
}

//衝突ペアを登録
void CollisionSystem::RegisterCollisionPair(Collider* colliderA, Collider* colliderB)
{
	m_currentCollisionPairs.push_back({ colliderA, colliderB });
}

//前回の衝突ペアと比較して新規衝突か継続衝突かをチェック
void CollisionSystem::UpdateCollisionState()
{
	//今回の衝突ペア配列をループ
	for (auto& pair : m_currentCollisionPairs)
	{
		//前回の衝突ペア配列に存在するかチェック
		bool isExist = PairExistsinList(pair, m_previousCollisionPairs);

		//衝突状態を設定
		auto state = isExist ? COLLISION_STATE::COLLISION_STAY : COLLISION_STATE::COLLISION_ENTER;

		SetCollisionState(
			pair.colliderA,	//コライダーA
			pair.colliderB,	//コライダーB
			state			//衝突状態
		);
		SetCollisionState(
			pair.colliderB,	//コライダーA
			pair.colliderA,	//コライダーB
			state			//衝突状態
		);
	}

	//前回の衝突ペア配列をループ
	for (auto& pair : m_previousCollisionPairs)
	{
		//今回の衝突ペア配列に存在するかチェック
		bool isExist = PairExistsinList(pair, m_currentCollisionPairs);

		if (!isExist)
		{//衝突終了
			CollisionInfo infoA{};
			infoA.opponent = pair.colliderB;
			infoA.contactPoint = { 0,0,0 };
			infoA.contactNormal = { 0,0,0 };
			infoA.penetrationDepth = { 0,0,0 };
			infoA.state = COLLISION_STATE::COLLISION_EXIT;
			pair.colliderA->AddCollisionInfo(infoA);

			CollisionInfo infoB = infoA;
			infoB.opponent = pair.colliderA;
			pair.colliderB->AddCollisionInfo(infoB);
		}
	}

	//前回の衝突ペア配列を今回の衝突ペア配列で更新
	m_previousCollisionPairs = m_currentCollisionPairs;

	//今回の衝突ペア配列クリア
	m_currentCollisionPairs.clear();
}

//コライダーのクリア
void CollisionSystem::ClearColliders()
{
	m_pCollidersList.clear();
}

//レイキャスト
void CollisionSystem::RaycastSegmentQuery(
	RaycastSegment& ray	//レイ情報
)
{
	ray.prevHitInfos = ray.currHitInfos;	//前回のヒット情報配列に今回のヒット情報配列をコピー
	ray.currHitInfos.clear();				//ヒット情報配列クリア

	for(auto& collider : m_pCollidersList)
	{
		//非アクティブなコライダーはスキップ
		if (!collider->isActive()) continue;
		
		//レイヤーチェック
		if (!CheckLayerRaycast(ray, collider)) continue;

		RaycastHitInfo info{};	//レイキャストヒット情報構造体
		bool hit = false;		//ヒットフラグ

		//コライダータイプに応じたレイキャスト関数の呼び出し
		switch (collider->GetType())
		{
		case ColliderType::BOX:		//ボックスコライダー
			hit = RaycastBox(
				ray,		//レイ情報
				collider,	//コライダー
				info		//ヒット情報
			);
			break;

		case ColliderType::SPHERE:	//球コライダー
			hit = RaycastSphere(
				ray,		//レイ情報
				collider,	//コライダー
				info		//ヒット情報
			);
			break;

		case ColliderType::CAPSULE:	//カプセルコライダー
			hit = RaycastCapsule(
				ray,		//レイ情報
				collider,	//コライダー
				info		//ヒット情報
			);
			break;

		default:
			break;
		}

		if (hit)
		{
			ray.currHitInfos.push_back(info);	//ヒット時にヒット情報配列に追加
		}
	}

	//ヒット情報配列を距離でソート
	std::sort(
		ray.currHitInfos.begin(),
		ray.currHitInfos.end(),
		[](const RaycastHitInfo& a, const RaycastHitInfo& b) {
			return a.hitDistance < b.hitDistance;	//距離が近い順にソート
		}
	);

	//衝突状態更新
	UpdateRaycastCollisionState(ray);
}

//AABBの衝突判定
bool CollisionSystem::CollisionAABB(
	Collider* colliderA,	//コライダーA
	Collider* colliderB		//コライダーB
)
{
	//AABB同士の当たり判定
	AABB aabbA = colliderA->GetSewptAABB();	//コライダーAのAABB取得
	AABB aabbB = colliderB->GetSewptAABB();	//コライダーBのAABB取得

	//衝突検知
	if (!(aabbA.min.x <= aabbB.max.x && aabbA.max.x >= aabbB.min.x)) return false;	//X軸方向
	if (!(aabbA.min.y <= aabbB.max.y && aabbA.max.y >= aabbB.min.y)) return false;	//Y軸方向
	if (!(aabbA.min.z <= aabbB.max.z && aabbA.max.z >= aabbB.min.z)) return false;	//Z軸方向

	return true;
}

//ナローフェーズ用配列に衝突ペアを追加
void CollisionSystem::SendNarrowPhase(Collider* colliderA, Collider* colliderB)
{
	//ナローフェーズ用配列に追加
	CollisionPair pair;							//衝突ペア構造体
	pair.colliderA = colliderA;					//コライダーA
	pair.colliderB = colliderB;					//コライダーB
	m_pNarrowPhaseColliders.push_back(pair);	//ナローフェーズ用配列に追加
}

//コライダーからOBBを作成
OBB CollisionSystem::CreateOBB(Collider* collider, float alpha)
{
	OBB obb{};	//OBB構造体

	//中心
	Vector3 prevCenter = collider->GetWorldTransformPrevious().position;	//前回の中心座標
	Vector3 currCenter = collider->GetWorldTransformCurrent().position;		//今回の中心座標
	Vector3 center = prevCenter.Lerp(currCenter, alpha);					//LERP補間で中心座標を求める
	obb.center = XMVectorSet(
		center.x,
		center.y,
		center.z,
		0.0f
	);

	//半分のサイズ
	Vector3 prevScale = collider->GetWorldTransformPrevious().scale;	//前回のスケール
	Vector3 currScale = collider->GetWorldTransformCurrent().scale;		//今回のスケール
	auto scale = prevScale.Lerp(currScale, alpha);						//LERP補間でスケールを求める
	obb.halfSizes = Vector3(
		scale.x * 0.5f,
		scale.y * 0.5f,
		scale.z * 0.5f
	);

	//各軸の方向ベクトル(ローカル座標系の基底ベクトルを回転させて求める)
	Matrix4x4 R = collider->GetWorldTransformCurrent().rotation.ToRotationMatrix();

	//ローカル座標系の基底ベクトルを回転させて各軸の方向ベクトルを求める
	obb.axis[0] = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), R));
	obb.axis[1] = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), R));
	obb.axis[2] = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), R));

	return obb;
}

//コライダーからカプセルセグメントを作成
CapsuleSegment CollisionSystem::CreateCapsuleSegment(Collider* collider, float alpha)
{
	CapsuleSegment seg{};	//カプセルセグメント構造体
	const CapsuleCollider currCap = collider->GetCurrentCapsuleCollider(); //現在のカプセルコライダー取得
	const CapsuleCollider prevCap = collider->GetPreviousCapsuleCollider(); //前回のカプセルコライダー取得

	seg.radius = prevCap.radius + (currCap.radius - prevCap.radius) * alpha;					//LERP補間で半径を求める
	seg.pointA = LerpXMV(XMLoadFloat3(&prevCap.pointA), XMLoadFloat3(&currCap.pointA), alpha);	//端点A設定
	seg.pointB = LerpXMV(XMLoadFloat3(&prevCap.pointB), XMLoadFloat3(&currCap.pointB), alpha);	//端点B設定

	return seg;
}

//連続衝突検知が必要かどうかの判定
bool CollisionSystem::NeedsCCD(Collider* collider)
{
	const Vector3& prevCenter = collider->GetWorldTransformPrevious().position;	//前回の中心座標
	const Vector3& currCenter = collider->GetWorldTransformCurrent().position;	//今回の中心座標
	float distSq = LengthSqBetween(prevCenter, currCenter);			//移動距離の二乗

	float thresh = 0.0f;	//閾値
	ColliderType type = collider->GetType();	//コライダータイプ取得

	float minExtent = 0.0f; //最小寸法

	Vector3 scale = collider->GetWorldTransformCurrent().scale;	//現在のスケール

	//コライダータイプに応じて閾値を設定
	switch (type)
	{
	case ColliderType::BOX:		//ボックスコライダー
		minExtent = (std::min)(scale.x, (std::min)(scale.y, scale.z));
		thresh = minExtent * 0.25f;
		break;
	case ColliderType::SPHERE:	//球コライダー
		thresh = collider->GetCurrentSphereCollider().radius * 0.5f;
		break;
	case ColliderType::CAPSULE:	//カプセルコライダー
		thresh = collider->GetCurrentCapsuleCollider().radius * 0.5f;
		break;
	default:
		thresh = 0.1f;
		break;
	}

	return distSq > thresh * thresh;	//閾値を超えている場合はtrueを返す
}

//サブステップ数の計算
int CollisionSystem::CalculateSubsteps(Collider* colliderA, Collider* colliderB)
{
	//移動距離を計算するラムダ式
	auto disp = [&](Collider* collider) {
		const Vector3& prevCenter = collider->GetWorldTransformPrevious().position;	//前回の中心座標
		const Vector3& currCenter = collider->GetWorldTransformCurrent().position;	//今回の中心座標
		return LengthBetween(prevCenter, currCenter);						//移動距離
		};

	//最大移動距離を計算
	float maxDisp = (std::max)(disp(colliderA), disp(colliderB));

	//ステップ長を計算するラムダ式
	auto stepLen = [&](Collider* collider) {
		ColliderType type = collider->GetType();	//コライダータイプ取得
		switch (type)
		{
		case ColliderType::BOX:		//ボックスコライダー
		{
			auto scale = collider->GetWorldTransformCurrent().scale;
			float minExtent = (std::min)(scale.x, (std::min)(scale.y, scale.z));
			return minExtent * 0.25f;
		}
		case ColliderType::SPHERE:	//球コライダー
			return collider->GetCurrentSphereCollider().radius * 0.5f;
		case ColliderType::CAPSULE:	//カプセルコライダー
			return collider->GetCurrentCapsuleCollider().radius * 0.5f;
		default:
			return 0.1f;
		}
		};

	//ステップ長の計算
	float step = (std::min)(stepLen(colliderA), stepLen(colliderB));
	step = (std::max)(step, 0.001f); //最小値でクランプ

	//サブステップ数計算
	int n = static_cast<int>(ceilf(maxDisp / step));

	return (std::min)((std::max)(n, 1), 32);	//1から32の範囲にクランプして返す
}

//ボックス同士の衝突判定
ContactResult CollisionSystem::CollisionBoxToBox(Collider* colliderA, Collider* colliderB)
{
	ContactResult result{};	//衝突結果構造体

	OBB a = CreateOBB(colliderA);	//コライダーAからOBB作成
	OBB b = CreateOBB(colliderB);	//コライダーBからOBB作成

	Vector3 ea = a.halfSizes;	//コライダーAの各軸方向の半分のサイズ
	Vector3 eb = b.halfSizes;	//コライダーBの各軸方向の半分のサイズ

	XMVECTOR tWorld = XMVectorSubtract(b.center, a.center); //コライダーAから見たコライダーBの位置ベクトル

	float minOverlap = FLT_MAX;			//最小貫入深さ
	XMVECTOR minAxis = XMVectorZero();	//最小貫入深さの軸

	//コライダーAのローカル座標系で見たコライダーBの位置ベクトルを計算
	float t[3]; //コライダーAのローカル座標系で見たコライダーBの位置ベクトル
	for (int i = 0; i < 3; ++i)
	{//各軸について内積計算
		t[i] = XMVectorGetX(XMVector3Dot(tWorld, a.axis[i]));
	}

	//回転行列の計算
	float R[3][3];					//各軸の方向ベクトルの内積を格納する配列
	float absR[3][3];				//絶対値を格納する配列
	const float EPSILON = 0.0001f;	//ゼロ除算防止用の微小値
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			R[i][j] = XMVectorGetX(XMVector3Dot(a.axis[i], b.axis[j]));	//各軸の方向ベクトルの内積計算
			absR[i][j] = fabsf(R[i][j]) + EPSILON;						//絶対値計算
		}
	}

	//分離軸の判定
	float eaArr[3] = { ea.x, ea.y, ea.z };	//コライダーAの各軸
	float ebArr[3] = { eb.x, eb.y, eb.z };	//コライダーBの各軸

	//コライダーAの各軸
	for (int i = 0; i < 3; ++i)
	{
		float ra = eaArr[i];
		float rb =
			ebArr[0] * absR[i][0] +
			ebArr[1] * absR[i][1] +
			ebArr[2] * absR[i][2];

		float overlap = ra + rb - fabsf(t[i]);

		if (overlap < 0) return result; // 分離軸あり

		if (overlap < minOverlap || minOverlap == 0)
		{
			minOverlap = overlap;
			minAxis = a.axis[i];
		}

	}
	//コライダーBの各軸
	for (int i = 0; i < 3; ++i)
	{
		float ra =
			eaArr[0] * absR[0][i] +
			eaArr[1] * absR[1][i] +
			eaArr[2] * absR[2][i];
		float rb = ebArr[i];

		float tProj = fabsf(
			t[0] * R[0][i] +
			t[1] * R[1][i] +
			t[2] * R[2][i]);

		float overlap = ra + rb - tProj;

		if (overlap < 0) return result; // 分離軸あり

		if (overlap < minOverlap || minOverlap == 0)
		{
			minOverlap = overlap;
			minAxis = b.axis[i];
		}

	}

	//交差軸の更新ラムダ式
	auto UpdateMinAxis = [&](XMVECTOR axis, float overlap) {
		const float eps = 1e-6f;	//微小値
		float lenSq = XMVectorGetX(XMVector3LengthSq(axis));
		if (lenSq < eps) return;	// ほぼ平行で無効
		if (overlap < minOverlap) {
			minOverlap = overlap;
			minAxis = axis;			// そのAi×Bjを採用
		}
		};

	//交差軸の判定
	//A0 x B0
	{
		float ra = ea.y * absR[2][0] + ea.z * absR[1][0];
		float rb = eb.y * absR[0][2] + eb.z * absR[0][1];
		float tProj = fabs(t[2] * R[1][0] - t[1] * R[2][0]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[0], b.axis[0]);
		UpdateMinAxis(axis, overlap);
	}
	//A0 x B1
	{
		float ra = ea.y * absR[2][1] + ea.z * absR[1][1];
		float rb = eb.x * absR[0][2] + eb.z * absR[0][0];
		float tProj = fabs(t[2] * R[1][1] - t[1] * R[2][1]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[0], b.axis[1]);
		UpdateMinAxis(axis, overlap);
	}
	//A0 x B2
	{
		float ra = ea.y * absR[2][2] + ea.z * absR[1][2];
		float rb = eb.x * absR[0][1] + eb.y * absR[0][0];
		float tProj = fabs(t[2] * R[1][2] - t[1] * R[2][2]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[0], b.axis[2]);
		UpdateMinAxis(axis, overlap);
	}
	//A1 x B0
	{
		float ra = ea.x * absR[2][0] + ea.z * absR[0][0];
		float rb = eb.y * absR[1][2] + eb.z * absR[1][1];
		float tProj = fabs(t[0] * R[2][0] - t[2] * R[0][0]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[1], b.axis[0]);
		UpdateMinAxis(axis, overlap);
	}
	//A1 x B1
	{
		float ra = ea.x * absR[2][1] + ea.z * absR[0][1];
		float rb = eb.x * absR[1][2] + eb.z * absR[1][0];
		float tProj = fabs(t[0] * R[2][1] - t[2] * R[0][1]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[1], b.axis[1]);
		UpdateMinAxis(axis, overlap);
	}
	//A1 x B2
	{
		float ra = ea.x * absR[2][2] + ea.z * absR[0][2];
		float rb = eb.x * absR[1][1] + eb.y * absR[1][0];
		float tProj = fabs(t[0] * R[2][2] - t[2] * R[0][2]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[1], b.axis[2]);
		UpdateMinAxis(axis, overlap);
	}
	//A2 x B0
	{
		float ra = ea.x * absR[1][0] + ea.y * absR[0][0];
		float rb = eb.y * absR[2][2] + eb.z * absR[2][1];
		float tProj = fabs(t[1] * R[0][0] - t[0] * R[1][0]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[2], b.axis[0]);
		UpdateMinAxis(axis, overlap);
	}
	//A2 x B1
	{
		float ra = ea.x * absR[1][1] + ea.y * absR[0][1];
		float rb = eb.x * absR[2][2] + eb.z * absR[2][0];
		float tProj = fabs(t[1] * R[0][1] - t[0] * R[1][1]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[2], b.axis[1]);
		UpdateMinAxis(axis, overlap);
	}
	//A2 x B2
	{
		float ra = ea.x * absR[1][2] + ea.y * absR[0][2];
		float rb = eb.x * absR[2][1] + eb.y * absR[2][0];
		float tProj = fabs(t[1] * R[0][2] - t[0] * R[1][2]);
		float overlap = ra + rb - tProj;
		if (overlap < 0) return result; //分離軸あり
		XMVECTOR axis = XMVector3Cross(a.axis[2], b.axis[2]);
		UpdateMinAxis(axis, overlap);
	}

	//最小軸の向きを調整
	XMVECTOR centerDiff = XMVectorSubtract(b.center, a.center);
	if (XMVectorGetX(XMVector3Dot(centerDiff, minAxis)) < 0)
	{
		minAxis = XMVectorNegate(minAxis);
	}

	//ここまで来たら衝突検知
	//衝突時パラメータの計算
	XMVECTOR point;					//衝突点
	XMVECTOR normal;				//法線
	float depth;					//貫入深さ

	//衝突点の計算(簡易的にコライダーAとコライダーBの中心点の中間点を衝突点とする)
	point = XMVectorScale(
		XMVectorAdd(a.center, b.center),
		0.5f
	);
	//法線ベクトル
	normal = XMVector3Normalize(minAxis);
	//貫入深さ
	depth = minOverlap;

	//衝突情報の作成
	result.isCollided = true;	//衝突検知フラグON
	result.point = point;		//衝突点
	result.normal = normal;		//法線
	result.depth = depth;		//貫入深さ

	return result;
}

//球同士の衝突判定
ContactResult CollisionSystem::CollisionSphereToSphere(
	Collider* colliderA,	//コライダーA
	Collider* colliderB		//コライダーB
)
{
	ContactResult result{};	//衝突結果構造体
	Vector3 centerA = colliderA->GetCurrentSphereCollider().center; //コライダーAの中心座標
	Vector3 centerB = colliderB->GetCurrentSphereCollider().center; //コライダーBの中心座標

	SphereSegment segA, segB;	//球セグメント構造体
	segA.center = XMLoadFloat3(&centerA);						//コライダーAの中心座標
	segA.radius = colliderA->GetCurrentSphereCollider().radius;	//コライダーAの半径
	segB.center = XMLoadFloat3(&centerB);						//コライダーBの中心座標
	segB.radius = colliderB->GetCurrentSphereCollider().radius;	//コライダーBの半径

	//球セグメント同士の衝突判定
	result = CollisionSpheresSegments(segA, segB);

	return result;
}

//カプセル同士の衝突判定
ContactResult CollisionSystem::CollisionCapsuleToCapsule(
	Collider* colliderA,	//コライダーA
	Collider* colliderB		//コライダーB
)
{
	ContactResult result{};	//衝突結果構造体

	CapsuleSegment segA = CreateCapsuleSegment(colliderA);	//コライダーAからカプセルセグメント作成
	CapsuleSegment segB = CreateCapsuleSegment(colliderB);	//コライダーBからカプセルセグメント作成

	const float epsilon = 0.0001f;	//微小値

	DirectX::XMVECTOR axisA = XMVectorSubtract(segA.pointB, segA.pointA); //コライダーAの軸ベクトル
	DirectX::XMVECTOR axisB = XMVectorSubtract(segB.pointB, segB.pointA); //コライダーBの軸ベクトル

	float lenA = XMVectorGetX(XMVector3Dot(axisA, axisA)); //コライダーAの軸ベクトルの長さ
	float lenB = XMVectorGetX(XMVector3Dot(axisB, axisB)); //コライダーBの軸ベクトルの長さ

	//軸ベクトルの長さが極端に短い場合の処理
	if (lenA < epsilon || lenB < epsilon)
	{//長さが極端に短い場合は球体として扱う
		SphereSegment sphereA, sphereB;	//球セグメント構造体

		sphereA.center = XMVectorScale(XMVectorAdd(segA.pointA, segA.pointB), 0.5f); //コライダーAの中心点
		sphereA.radius = segA.radius;													//コライダーAの半径
		sphereB.center = XMVectorScale(XMVectorAdd(segB.pointA, segB.pointB), 0.5f); //コライダーBの中心点
		sphereB.radius = segB.radius;													//コライダーBの半径

		//球セグメント同士の衝突判定
		result = CollisionSpheresSegments(sphereA, sphereB);
		return result;
	}

	//最短距離の二乗を取得
	XMVECTOR closestA, closestB;	//コライダーA・B上の最短点
	float distSq = CollisionSystem::GetMinDistanceSquaredSegmentToSegment(
		segA.pointA, segA.pointB,	//セグメントAの端点
		segB.pointA, segB.pointB,	//セグメントBの端点
		closestA,					//セグメントA上の最短点
		closestB					//セグメントB上の最短点
	);

	//衝突検知
	float radiusSum = segA.radius + segB.radius;			//半径の和
	if (!(distSq <= radiusSum * radiusSum)) return result;	//衝突なし

	//衝突時パラメータの計算
	XMVECTOR point;		//衝突点
	XMVECTOR normal;	//法線
	float depth;		//貫入深さ

	//衝突点の計算(最短点の中間)
	point = XMVectorScale(XMVectorAdd(closestA, closestB), 0.5f);

	//法線ベクトルの計算
	float dist = sqrtf(distSq); //最短距離
	if (dist < epsilon)
	{//最短距離がほぼ0の場合の処理
		//中心点の差ベクトルを正規化して法線ベクトルとする
		XMVECTOR centerA = XMVectorScale(XMVectorAdd(segA.pointA, segA.pointB), 0.5f); //コライダーAの中心点
		XMVECTOR centerB = XMVectorScale(XMVectorAdd(segB.pointA, segB.pointB), 0.5f); //コライダーBの中心点
		normal = XMVector3Normalize(XMVectorSubtract(centerB, centerA));
	}
	else if (dist == 0.0f)
	{//最短距離が完全に0の場合の処理
		//適当な法線ベクトルを設定
		normal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}
	else
	{
		//ベクトルの引き算と正規化
		normal = XMVector3Normalize(XMVectorSubtract(closestB, closestA));
	}

	//貫入深さの計算
	depth = radiusSum - dist;

	//衝突情報の作成
	result.isCollided = true;	//衝突検知フラグON
	result.point = point;		//衝突点
	result.normal = normal;		//法線
	result.depth = depth;		//貫入深さ

	return result;
}

//ボックスと球の衝突判定
ContactResult CollisionSystem::CollisionBoxToSphere(
	Collider* box,		//ボックスコライダー
	Collider* sphere	//スフィアコライダー
)
{
	ContactResult result{};	//衝突結果構造体

	OBB obb = CreateOBB(box);	//ボックスコライダーからOBB作成

	//OBB情報
	Vector3 boxCenter;		//OBBの中心点
	Vector3 boxHalfSizes;	//OBBの各軸方向の半分のサイズ
	XMVECTOR boxAxis[3];	//OBBの各軸の方向ベクトル

	//球情報
	Vector3 sphereCenter;	//球の中心点
	float sphereRadius;		//球の半径

	//OBB情報の取得
	XMStoreFloat3(&boxCenter, obb.center);	//OBBの中心点取得
	boxHalfSizes = obb.halfSizes;			//OBBの各軸方向の半分のサイズ取得
	for (int i = 0; i < 3; ++i)				//OBBの各軸の方向ベクトル取得
	{
		boxAxis[i] = obb.axis[i];
	}

	//球情報の取得
	sphereCenter = sphere->GetCurrentSphereCollider().center;	//球の中心点取得
	sphereRadius = sphere->GetCurrentSphereCollider().radius;	//球の半径取得

	//球の中心点をOBBのローカル座標系で表現
	XMVECTOR boxCenterV = XMLoadFloat3(&boxCenter);			//OBBの中心点ベクトル
	XMVECTOR sphereCenterV = XMLoadFloat3(&sphereCenter);	//球の中心点ベクトル

	XMVECTOR d = XMVectorSubtract(sphereCenterV, boxCenterV); //OBBの中心点から球の中心点へのベクトル

	float localX = XMVectorGetX(XMVector3Dot(d, boxAxis[0])); //OBBのローカルX座標
	float localY = XMVectorGetX(XMVector3Dot(d, boxAxis[1])); //OBBのローカルY座標
	float localZ = XMVectorGetX(XMVector3Dot(d, boxAxis[2])); //OBBのローカルZ座標

	//最も近い点をOBBのローカル座標系で計算
	float closestX = (std::max)(-boxHalfSizes.x, (std::min)(boxHalfSizes.x, localX));
	float closestY = (std::max)(-boxHalfSizes.y, (std::min)(boxHalfSizes.y, localY));
	float closestZ = (std::max)(-boxHalfSizes.z, (std::min)(boxHalfSizes.z, localZ));

	//最も近い点と球の中心点の差ベクトルを計算
	float diffX = closestX - localX; //最も近い点と球の中心点のX成分の差
	float diffY = closestY - localY; //最も近い点と球の中心点のY成分の差
	float diffZ = closestZ - localZ; //最も近い点と球の中心点のZ成分の差

	//最短距離の二乗を計算
	float distSq = diffX * diffX + diffY * diffY + diffZ * diffZ;
	float radiusSq = sphereRadius * sphereRadius; //球の半径の二乗

	//衝突検知
	if (distSq > radiusSq) return result; //衝突なし

	//衝突時パラメータの計算
	XMVECTOR point;		//衝突点
	XMVECTOR normal;	//法線
	float depth;		//貫入深さ

	//最も近い点をワールド座標系で計算
	XMVECTOR closestWorld; //最も近い点のワールド座標系での位置ベクトル
	closestWorld = XMVectorAdd(
		boxCenterV,
		XMVectorAdd(
			XMVectorScale(boxAxis[0], closestX),
			XMVectorAdd(
				XMVectorScale(boxAxis[1], closestY),
				XMVectorScale(boxAxis[2], closestZ)
			)
		)
	);

	point = closestWorld; //衝突点は最も近い点とする

	//法線ベクトルの計算
	const float epsilon = 0.0001f;	//微小値

	//球中心がOBB内かどうかで場合分け
	if (distSq < epsilon)	//球中心がOBB内（最近点が中心と一致）
	{
		//各面までの距離（ローカル空間）
		float sx = boxHalfSizes.x - fabsf(localX);
		float sy = boxHalfSizes.y - fabsf(localY);
		float sz = boxHalfSizes.z - fabsf(localZ);

		float distToFace;	//中心→最近面までの距離

		//最近面（最小距離の軸）を選ぶ
		if (sx <= sy && sx <= sz)
		{
			normal = (localX >= 0.0f) ? boxAxis[0] : XMVectorNegate(boxAxis[0]);
			closestX = (localX >= 0.0f) ? boxHalfSizes.x : -boxHalfSizes.x;
			closestY = localY;
			closestZ = localZ;
			distToFace = sx;
		}
		else if (sy <= sz)
		{
			normal = (localY >= 0.0f) ? boxAxis[1] : XMVectorNegate(boxAxis[1]);
			closestY = (localY >= 0.0f) ? boxHalfSizes.y : -boxHalfSizes.y;
			closestX = localX;
			closestZ = localZ;
			distToFace = sy;
		}
		else
		{
			normal = (localZ >= 0.0f) ? boxAxis[2] : XMVectorNegate(boxAxis[2]);
			closestZ = (localZ >= 0.0f) ? boxHalfSizes.z : -boxHalfSizes.z;
			closestX = localX;
			closestY = localY;
			distToFace = sz;
		}

		//最近面上の点をワールドに再構築
		closestWorld = XMVectorAdd(
			boxCenterV,
			XMVectorAdd(
				XMVectorScale(boxAxis[0], closestX),
				XMVectorAdd(
					XMVectorScale(boxAxis[1], closestY),
					XMVectorScale(boxAxis[2], closestZ)
				)
			)
		);

		point = closestWorld;				//衝突点は箱表面の最近面点
		depth = sphereRadius + distToFace;	//内部押し出しの貫入深さ
	}
	else
	{
		point = closestWorld; //衝突点

		//最短距離の計算
		float dist = sqrtf(distSq);	//最短距離

		if (dist > epsilon)
		{//法線ベクトル計算
			normal = XMVectorScale(
				XMVectorSubtract(sphereCenterV, closestWorld),
				1.0f / dist
			);
		}
		else
		{//最短距離が極端に小さい場合
			normal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); //適当な法線ベクトル
		}

		//貫入深さの計算
		depth = sphereRadius - dist; //貫入深さ
	}

	//衝突情報の作成
	result.isCollided = true;	//衝突検知フラグON
	result.point = point;		//衝突点
	result.normal = normal;		//法線
	result.depth = depth;		//貫入深さ

	return result;
}

//ボックスとカプセルの衝突判定
ContactResult CollisionSystem::CollisionBoxToCapsule(
	Collider* box,	//コライダーA
	Collider* capsule		//コライダーB
)
{
	ContactResult result{};	//衝突結果構造体

	OBB obb = CreateOBB(box);								//ボックスコライダーからOBB作成
	CapsuleSegment cupSeg = CreateCapsuleSegment(capsule);	//カプセルコライダーからカプセルセグメント作成

	result = CollisonOBBtoCapsule(obb, cupSeg);	//OBB対カプセルの衝突判定

	return result;
}

//球とカプセルの衝突判定
ContactResult CollisionSystem::CollisionSphereToCapsule(
	Collider* sphere,	//球コライダー
	Collider* capsule	//カプセルコライダー
)
{
	ContactResult result{};	//衝突結果構造体

	CapsuleSegment cupSeg = CreateCapsuleSegment(capsule);		//カプセルコライダーからカプセルセグメント作成
	SphereCollider sphereCol = sphere->GetCurrentSphereCollider();		//球コライダー情報取得
	XMVECTOR sphereCenter = XMLoadFloat3(&sphereCol.center);	//球の中心点ベクトル
	float radius = sphereCol.radius;							//球の半径

	//最短距離の二乗を取得
	XMVECTOR closestPoint; //カプセルセグメント上の最短点
	float distSq = CollisionSystem::GetMinDistanceSquaredPointToSegment(
		sphereCenter,				//点(球の中心点)
		cupSeg.pointA,				//カプセルセグメントの端点A
		cupSeg.pointB,				//カプセルセグメントの端点B
		closestPoint				//カプセルセグメント上の最短点
	);

	float radiusSum = radius + cupSeg.radius;				//半径の和
	if (!(distSq <= radiusSum * radiusSum)) return result;	//衝突なし

	//衝突時パラメータの計算
	const float epsilon = 0.0001f;	//微小値
	XMVECTOR point;					//衝突点
	XMVECTOR normal;				//法線
	float depth;					//貫入深さ

	XMVECTOR diff;	//最短点と球の中心点の差ベクトル
	float dist;		//最短距離
	diff = XMVectorSubtract(closestPoint, sphereCenter);
	dist = XMVectorGetX(XMVector3Length(diff));

	//法線ベクトルの計算
	if (dist > epsilon)
	{//法線ベクトル計算
		normal = XMVectorScale(
			diff,
			1.0f / dist
		);
	}
	else
	{//最短距離が極端に小さい場合
		normal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); //適当な法線ベクトル
	}

	depth = radiusSum - dist;								//貫入深さ計算

	//衝突点の計算(最短点と球の中心点の中間)
	Vector3 surfacePoint; //球の表面上の点
	surfacePoint = {
		XMVectorGetX(sphereCenter) + XMVectorGetX(normal) * radius,
		XMVectorGetY(sphereCenter) + XMVectorGetY(normal) * radius,
		XMVectorGetZ(sphereCenter) + XMVectorGetZ(normal) * radius
	};
	Vector3 surfaceCapsule; //カプセルの表面上の点
	surfaceCapsule = {
		XMVectorGetX(closestPoint) - XMVectorGetX(normal) * cupSeg.radius,
		XMVectorGetY(closestPoint) - XMVectorGetY(normal) * cupSeg.radius,
		XMVectorGetZ(closestPoint) - XMVectorGetZ(normal) * cupSeg.radius
	};
	point = XMVectorScale(
		XMVectorAdd(
			XMLoadFloat3(&surfacePoint),
			XMLoadFloat3(&surfaceCapsule)
		),
		0.5f
	);

	//衝突情報の作成
	result.isCollided = true;	//衝突検知フラグON
	result.point = point;		//衝突点
	result.normal = normal;		//法線
	result.depth = depth;		//貫入深さ

	return result;
}

//ボックスとカプセルのCCD衝突判定
ContactResult CollisionSystem::CollisionBoxToCapsuleCCD(Collider* box, Collider* capsule)
{
	int steps = CalculateSubsteps(box, capsule);	//サブステップ数計算
	ContactResult hit{};
	float alphaHit = -1.0f;
	float alphaPrev = 0.0f;

	//サブステップごとに衝突判定
	for (int i = 1; i <= steps; i++)
	{
		float alpha = static_cast<float>(i) / static_cast<float>(steps);	//補間パラメータ

		OBB obb = CreateOBB(box, alpha); ;							//補間後のOBB作成
		CapsuleSegment seg = CreateCapsuleSegment(capsule, alpha);	//補間後のカプセルセグメント作成


		ContactResult result = CollisonOBBtoCapsule(obb, seg);	//OBB対カプセルの衝突判定

		if (result.isCollided)
		{
			alphaHit = alpha;
			hit = result;
			break;
		}
		alphaPrev = alpha;
	}

	if (alphaHit < 0.0f) return {};

	// 2) ちょい精度上げたいなら二分探索でTOIを詰める（任意）
	float lo = alphaPrev, hi = alphaHit;
	for (int k = 0; k < 6; k++)
	{
		float mid = (lo + hi) * 0.5f;
		ContactResult tmp = CollisonOBBtoCapsule(CreateOBB(box, mid),
			CreateCapsuleSegment(capsule, mid));
		if (tmp.isCollided) { hi = mid; hit = tmp; }
		else lo = mid;
	}
	alphaHit = hi;

	// 3) ヒット後に法線方向へ進んだ量を depth に反映
	XMVECTOR n = hit.normal;
	Vector3 ccPrev = capsule->GetWorldTransformPrevious().position;
	Vector3 ccCurr = capsule->GetWorldTransformCurrent().position;
	Vector3 bcPrev = box->GetWorldTransformPrevious().position;
	Vector3 bcCurr = box->GetWorldTransformCurrent().position;

	XMVECTOR cPrev = XMLoadFloat3(&ccPrev);
	XMVECTOR cCurr = XMLoadFloat3(&ccCurr);
	XMVECTOR bPrev = XMLoadFloat3(&bcPrev);
	XMVECTOR bCurr = XMLoadFloat3(&bcCurr);

	// 相対位置で見る（両方動く可能性を考慮）
	XMVECTOR relPrev = XMVectorSubtract(cPrev, bPrev);
	XMVECTOR relCurr = XMVectorSubtract(cCurr, bCurr);
	XMVECTOR relHit = XMVectorLerp(relPrev, relCurr, alphaHit);

	float remaining = XMVectorGetX(
		XMVector3Dot(XMVectorSubtract(relCurr, relHit), n)
	);

	// 現在姿勢でもオーバーラップがあるならそれも見る
	ContactResult curr = CollisonOBBtoCapsule(CreateOBB(box, 1.0f),
		CreateCapsuleSegment(capsule, 1.0f));
	float currDepth = curr.isCollided ? curr.depth : 0.0f;

	hit.depth = (std::max)({ hit.depth, currDepth, remaining });

	return hit;
}

//OBB対カプセルの衝突判定
ContactResult CollisionSystem::CollisonOBBtoCapsule(const OBB& obb, const CapsuleSegment& capSeg)
{
	ContactResult result{};	//衝突結果構造体

	const float radius = capSeg.radius; //カプセルの半径

	XMVECTOR collisionPoints = XMVectorZero();	//衝突点の合計ベクトル
	XMVECTOR collisionNormals = XMVectorZero();	//法線ベクトルの合計ベクトル

	XMVECTOR A = capSeg.pointA;				//カプセルセグメントの端点A
	XMVECTOR B = capSeg.pointB;				//カプセルセグメントの端点B
	XMVECTOR AB = XMVectorSubtract(B, A);	//カプセルセグメントの方向ベクトル

	float segLen = XMVectorGetX(XMVector3Length(AB));			//カプセルセグメントの長さ
	int sampleCount = (int)ceilf(segLen / (radius * 0.5f));		//サンプリング数の計算

	float minDistSq = FLT_MAX;					//最短距離の二乗の最小値
	XMVECTOR  minClosestPoint = XMVectorZero();	//最短距離の最小値のときのOBB上の最短点
	XMVECTOR  minSamplePoint = XMVectorZero();	//最短距離の最小値のときのカプセルセグメント上のサンプリング点

	//カプセルセグメント上をサンプリングしてOBBとの最短距離を計算
	for (int i = 0; i < sampleCount; ++i)
	{
		float t = 0.0f;	//パラメータt
		if (sampleCount > 1)
		{//パラメータtを計算
			t = static_cast<float>(i) / static_cast<float>(sampleCount - 1); //パラメータt
		}

		//カプセルセグメント上のサンプリング点を計算
		XMVECTOR sampleCenter = XMVectorAdd(
			A,
			XMVectorScale(AB, t)
		);

		//最短距離の二乗を取得
		XMVECTOR closestPoint; //OBB上の最短点
		float distSq = GetMinDistanceSquaredPointToOBB(
			sampleCenter,	//点(サンプリング点)
			obb,			//OBB
			closestPoint	//OBB上の最短点
		);

		//最短距離の最小値を更新
		if (distSq < minDistSq)
		{
			minDistSq = distSq;
			minClosestPoint = closestPoint;
			minSamplePoint = sampleCenter;
		}

		if (distSq <= radius * radius)
		{//衝突検知
			collisionPoints = closestPoint;

			//法線ベクトルの計算
			XMVECTOR diff = XMVectorSubtract(sampleCenter, closestPoint); //最短点とサンプリング点の差ベクトル
			const float epsilon = 0.0001f;						//微小値
			float dist = XMVectorGetX(XMVector3Length(diff));	//最短距離

			if (dist > epsilon)
			{//法線ベクトル計算
				collisionNormals = XMVectorScale(
					diff,
					1.0f / dist
				);
			}
			else
			{
				collisionNormals = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); //適当な法線ベクトル
			}
		}
	}

	if (minDistSq > radius * radius) return result; //衝突なし

	//衝突時パラメータの計算
	XMVECTOR point;		//衝突点
	XMVECTOR normal;	//法線
	float depth;		//貫入深さ

	const float epsilon = 0.0001f;						//微小値
	float dist = sqrtf((std::max)(minDistSq, epsilon));	//最短距離

	// minSamplePoint を OBBローカルへ
	XMVECTOR dBox = XMVectorSubtract(minSamplePoint, obb.center);
	float localX = XMVectorGetX(XMVector3Dot(dBox, obb.axis[0]));
	float localY = XMVectorGetX(XMVector3Dot(dBox, obb.axis[1]));
	float localZ = XMVectorGetX(XMVector3Dot(dBox, obb.axis[2]));

	// ★ inside 判定を明示
	bool isInside =
		fabsf(localX) <= obb.halfSizes.x &&
		fabsf(localY) <= obb.halfSizes.y &&
		fabsf(localZ) <= obb.halfSizes.z;

	XMVECTOR normalVec;	//法線ベクトル
	float depthScalar;		//貫入深さスカラー値

	if (isInside)
	{//最短距離がほぼ0の場合の処理（カプセルセグメントのサンプリング点がOBB内部にある場合）
		//minSamplePointをOBBのローカル空間へ投影
		XMVECTOR dBox = XMVectorSubtract(minSamplePoint, obb.center);

		float localX = XMVectorGetX(XMVector3Dot(dBox, obb.axis[0]));
		float localY = XMVectorGetX(XMVector3Dot(dBox, obb.axis[1]));
		float localZ = XMVectorGetX(XMVector3Dot(dBox, obb.axis[2]));

		// 各面までの距離
		float sx = obb.halfSizes.x - fabsf(localX);
		float sy = obb.halfSizes.y - fabsf(localY);
		float sz = obb.halfSizes.z - fabsf(localZ);

		float distToFace;	//中心→最近面までの距離
		float cx = localX, cy = localY, cz = localZ;	//最近点のローカル座標

		//一番近い面の法線を決定（Box-Sphere 内部処理と同じ）
		if (sx <= sy && sx <= sz)
		{
			//±X面
			normalVec = (localX >= 0.0f) ? obb.axis[0] : XMVectorNegate(obb.axis[0]);
			cx = (localX >= 0.0f) ? obb.halfSizes.x : -obb.halfSizes.x;
			distToFace = sx;

		}
		else if (sy <= sz)
		{
			//±Y面
			normalVec = (localY >= 0.0f) ? obb.axis[1] : XMVectorNegate(obb.axis[1]);
			cy = (localY >= 0.0f) ? obb.halfSizes.y : -obb.halfSizes.y;
			distToFace = sy;
		}
		else
		{
			//±Z面
			normalVec = (localZ >= 0.0f) ? obb.axis[2] : XMVectorNegate(obb.axis[2]);
			cz = (localZ >= 0.0f) ? obb.halfSizes.z : -obb.halfSizes.z;
			distToFace = sz;
		}

		depthScalar = radius + distToFace;	//貫入深さ計算

		minClosestPoint = XMVectorAdd(
			obb.center,
			XMVectorAdd(
				XMVectorScale(obb.axis[0], cx),
				XMVectorAdd(
					XMVectorScale(obb.axis[1], cy),
					XMVectorScale(obb.axis[2], cz)
				)
			)
		);
	}
	else
	{
		//通常ケース：最短点とサンプル点の差から法線を計算
		XMVECTOR diff = XMVectorSubtract(minSamplePoint, minClosestPoint);
		normalVec = XMVectorScale(diff, 1.0f / dist);

		depthScalar = radius - dist;	//貫入深さ計算
	}

	depthScalar = (std::max)(0.0f, depthScalar);	//貫入深さ計算

	//カプセルの表面上の点
	XMVECTOR capsuleSurfacePoint = XMVectorSubtract(
		minSamplePoint,
		XMVectorScale(normalVec, radius)
	);

	//衝突点の計算(最短点とカプセル表面上の点の中間)
	XMVECTOR contactPoint = XMVectorScale(
		XMVectorAdd(
			minClosestPoint,
			capsuleSurfacePoint
		),
		0.5f
	);

	point = contactPoint;	//衝突点設定
	normal = normalVec;		//法線設定
	depth = depthScalar;	//貫入深さ設定

	//衝突情報の作成
	result.isCollided = true;	//衝突検知フラグON
	result.point = point;		//衝突点
	result.normal = normal;		//法線
	result.depth = depth;		//貫入深さ

	return result;
}

//球セグメント間の衝突判定
ContactResult CollisionSystem::CollisionSpheresSegments(const SphereSegment& segA, const SphereSegment& segB)
{
	ContactResult result{};	//衝突結果構造体

	//中心点の取得
	Vector3 centerA, centerB;
	XMStoreFloat3(&centerA, segA.center);
	XMStoreFloat3(&centerB, segB.center);

	//半径の取得
	float radiusA = segA.radius;
	float radiusB = segB.radius;
	float radiusSum = radiusA + radiusB;

	//中心点間の距離の計算
	float dist = sqrtf(
		(centerA.x - centerB.x) * (centerA.x - centerB.x) +
		(centerA.y - centerB.y) * (centerA.y - centerB.y) +
		(centerA.z - centerB.z) * (centerA.z - centerB.z)
	);

	//衝突検知
	if (!(dist <= radiusSum)) return result;

	//衝突時パラメータの計算
	const float epsilon = 0.0001f;	//微小値
	XMVECTOR point;					//衝突点
	XMVECTOR normal;				//法線
	float depth;					//貫入深さ

	//衝突点の計算
	point =
	{
		(centerA.x + centerB.x) / 2.0f,
		(centerA.y + centerB.y) / 2.0f,
		(centerA.z + centerB.z) / 2.0f
	};

	//法線ベクトル
	if (dist < epsilon)
	{//中心点がほぼ同じ位置にある場合の処理
		//適当な法線ベクトルを設定
		normal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}
	else
	{
		normal = XMVectorSet(
			(centerB.x - centerA.x) / dist,
			(centerB.y - centerA.y) / dist,
			(centerB.z - centerA.z) / dist,
			0.0f
		);
	}

	//貫入深さ
	depth = radiusSum - dist;

	//衝突情報の作成
	result.isCollided = true;	//衝突検知フラグON
	result.point = point;		//衝突点
	result.normal = normal;		//法線
	result.depth = depth;		//貫入深さ

	return result;
}

//セグメント間の最小距離の二乗を取得
float CollisionSystem::GetMinDistanceSquaredSegmentToSegment(
	const DirectX::FXMVECTOR& p0, const DirectX::FXMVECTOR& p1,	//セグメントPの端点
	const DirectX::FXMVECTOR& q0, const DirectX::FXMVECTOR& q1,	//セグメントQの端点
	DirectX::XMVECTOR& outP,									//セグメントP上の最短点
	DirectX::XMVECTOR& outQ										//セグメントQ上の最短点
)
{
	//セグメントPとセグメントQの各種ベクトル計算
	XMVECTOR dP = XMVectorSubtract(p1, p0);	//セグメントPの方向ベクトル
	XMVECTOR dQ = XMVectorSubtract(q1, q0);	//セグメントQの方向ベクトル
	XMVECTOR W = XMVectorSubtract(p0, q0);	//セグメントPの端点p0から見たセグメントQの端点q0へのベクトル

	//各種内積計算
	float a = XMVectorGetX(XMVector3Dot(dP, dP));	//セグメントPの方向ベクトルの長さの二乗
	float b = XMVectorGetX(XMVector3Dot(dP, dQ));	//セグメントPとセグメントQの方向ベクトルの内積
	float c = XMVectorGetX(XMVector3Dot(dQ, dQ));	//セグメントQの方向ベクトルの長さの二乗
	float d = XMVectorGetX(XMVector3Dot(dP, W));	//セグメントPの方向ベクトルとベクトルWの内積
	float e = XMVectorGetX(XMVector3Dot(dQ, W));	//セグメントQの方向ベクトルとベクトルWの内積

	const float EPSILON = 0.0001f;	//ゼロ除算防止用の微小値
	float denom = a * c - b * b;	//分母

	float s, t; //パラメータsとt

	if (denom < EPSILON)
	{//平行な場合
		s = 0.0f;	//セグメントP上の点はp0に固定
		t = e / c;	//セグメントQ上の点を計算
	}
	else
	{//平行でない場合
		s = (b * e - c * d) / denom;	 //セグメントP上の点を計算
		t = (a * e - b * d) / denom;	 //セグメントQ上の点を計算
	}

	//パラメータsとtをセグメントの範囲内にクランプ
	s = (std::max)(0.0f, (std::min)(1.0f, s));
	t = (std::max)(0.0f, (std::min)(1.0f, t));

	//最短点の計算
	outP = XMVectorAdd(p0, XMVectorScale(dP, s)); //セグメントP上の最短点
	outQ = XMVectorAdd(q0, XMVectorScale(dQ, t)); //セグメントQ上の最短点

	//最短距離の二乗の計算
	XMVECTOR diff = XMVectorSubtract(outP, outQ); //最短点同士の差ベクトル
	return XMVectorGetX(XMVector3Dot(diff, diff)); //最短距離の二乗を返す
}

//点とセグメント間の最小距離の二乗を取得
float CollisionSystem::GetMinDistanceSquaredPointToSegment(const DirectX::FXMVECTOR& point, const DirectX::FXMVECTOR& segA, const DirectX::FXMVECTOR& segB, DirectX::XMVECTOR& outClosest)
{
	XMVECTOR segDir = XMVectorSubtract(segB, segA);		//セグメントの方向ベクトル
	XMVECTOR toPoint = XMVectorSubtract(point, segA);	//セグメントの端点Aから点へのベクトル

	float segLengthSquared = XMVectorGetX(XMVector3Dot(segDir, segDir)); //セグメントの長さの二乗

	//セグメントの長さが極端に短い場合の処理
	const float epsilon = 0.0001f; //ゼロ除算防止用の微小値
	if (segLengthSquared < epsilon)
	{//セグメントの長さがほぼゼロの場合、端点Aを最短点とする
		outClosest = segA;
		XMVECTOR diff = XMVectorSubtract(point, segA);	//最短点と点の差ベクトル
		return XMVectorGetX(XMVector3Dot(diff, diff));	//最短距離の二乗を返す
	}

	float t = XMVectorGetX(XMVector3Dot(toPoint, segDir)) / segLengthSquared; //パラメータtの計算

	//パラメータtをセグメントの範囲内にクランプ
	t = (std::max)(0.0f, (std::min)(1.0f, t));

	//最短点の計算
	outClosest = XMVectorAdd(segA, XMVectorScale(segDir, t)); //セグメント上の最短点

	//最短距離の二乗の計算
	XMVECTOR diff = XMVectorSubtract(point, outClosest);	//最短点と点の差ベクトル
	return XMVectorGetX(XMVector3Dot(diff, diff));			//最短距離の二乗を返す
}

//点とOBB間の最小距離の二乗を取得
float CollisionSystem::GetMinDistanceSquaredPointToOBB(const DirectX::FXMVECTOR& point, const OBB& obb, DirectX::XMVECTOR& outClosest)
{
	XMVECTOR d = XMVectorSubtract(point, obb.center); //点からOBBの中心へのベクトル

	const Vector3& halfSizes = obb.halfSizes; //OBBの各軸方向の半分のサイズ

	//点のOBBのローカル座標系での位置を計算
	float local[3]; //OBBのローカル座標系での点の位置
	//各軸について内積計算
	for (int i = 0; i < 3; ++i)
	{
		local[i] = XMVectorGetX(XMVector3Dot(d, obb.axis[i]));
	}

	//クランプ処理
	float clamped[3]; //各軸方向にクランプした値
	clamped[0] = (std::max)(-halfSizes.x, (std::min)(halfSizes.x, local[0]));
	clamped[1] = (std::max)(-halfSizes.y, (std::min)(halfSizes.y, local[1]));
	clamped[2] = (std::max)(-halfSizes.z, (std::min)(halfSizes.z, local[2]));

	//最短点の計算
	outClosest = XMVectorAdd(
		obb.center,
		XMVectorAdd(
			XMVectorScale(obb.axis[0], clamped[0]),
			XMVectorAdd(
				XMVectorScale(obb.axis[1], clamped[1]),
				XMVectorScale(obb.axis[2], clamped[2])
			)
		)
	);

	//最短距離の二乗の計算
	XMVECTOR diff = XMVectorSubtract(point, outClosest);	//最短点と点の差ベクトル
	return XMVectorGetX(XMVector3Dot(diff, diff));			//最短距離の二乗を返す
}

//衝突ペアが保存されているかどうかチェック
bool CollisionSystem::PairExistsinList(const CollisionPair& pair, const std::vector<CollisionPair>& collisionPairs)
{
	//保存されている衝突ペアと比較
	for (auto& p : collisionPairs)
	{
		if (pair.colliderA == p.colliderA && pair.colliderB == p.colliderB
			|| pair.colliderA == p.colliderB && pair.colliderB == p.colliderA)
		{
			return true;
		}
	}
	return false;
}

//コライダーの衝突状態を設定
void CollisionSystem::SetCollisionState(Collider* self, Collider* opponent, COLLISION_STATE state)
{
	auto& infoList = self->GetCollisionInfos();
	//衝突情報リストから相手コライダーを探して状態を設定
	for (auto& info : infoList)
	{
		if (info.opponent == opponent)
		{
			info.state = state;
			return;
		}
	}
}

//コライダーに衝突情報を追加
void CollisionSystem::PushCollisionInfo(Collider* colliderA, Collider* colliderB, ContactResult& result)
{
	Vector3 contactP, normalF;
	XMStoreFloat3(&contactP, result.point);	//衝突点
	XMStoreFloat3(&normalF, result.normal);	//法線ベクトル

	Vector3 penetration =
	{//貫入深さベクトルの計算
		normalF.x * result.depth,
		normalF.y * result.depth,
		normalF.z * result.depth
	};

	//衝突情報の作成
	CollisionInfo infoA;					//衝突情報
	infoA.opponent = colliderB;				//衝突相手のコライダー
	infoA.contactPoint = contactP;			//衝突点
	infoA.contactNormal = normalF;			//法線
	infoA.penetrationDepth = penetration;	//貫入深さ
	colliderA->AddCollisionInfo(infoA);		//衝突情報を追加

	CollisionInfo infoB;					//衝突情報
	infoB.opponent = colliderA;				//衝突相手のコライダー
	infoB.contactPoint = contactP;			//衝突点
	infoB.contactNormal =					//反転法線
	{
		-normalF.x,
		-normalF.y,
		-normalF.z
	};
	infoB.penetrationDepth =				//反転貫入深さ
	{
		-penetration.x,
		-penetration.y,
		-penetration.z
	};
	colliderB->AddCollisionInfo(infoB);		//衝突情報を追加

	colliderA->SetDetected(true);
	colliderB->SetDetected(true);
}

//法線ベクトルをコライダーAからBの方向に向ける
void CollisionSystem::OrientNormalAToB(Collider* colliderA, Collider* colliderB, ContactResult& result)
{
	XMVECTOR normal = result.normal;	//法線ベクトル

	Vector3 centerAF = colliderA->GetWorldTransformCurrent().position;
	Vector3 centerBF = colliderB->GetWorldTransformCurrent().position;

	//コライダーA・Bの中心点ベクトルを取得
	XMVECTOR centerA = XMLoadFloat3(&centerAF);
	XMVECTOR centerB = XMLoadFloat3(&centerBF);

	//コライダーAからBへの方向ベクトルと法線ベクトルの内積を計算
	XMVECTOR dir = XMVectorSubtract(centerB, centerA);	//コライダーAからBへの方向ベクトル

	if (XMVectorGetX(XMVector3Dot(dir, normal)) < 0.0f)	//法線ベクトルがコライダーAからBの方向を向いていない場合
	{
		normal = XMVectorNegate(normal);	//法線ベクトルを反転
	}

	result.normal = normal;	//法線ベクトルを更新
}

//レイヤーマスクによるレイキャストの衝突判定
bool CollisionSystem::CheckLayerRaycast(const RaycastSegment& ray, Collider* collider)
{
	LayerMask rayBit = LayerToBit(ray.layer);				//レイのレイヤーマスク
	LayerMask colBit = LayerToBit(collider->GetLayer());	//コライダーのレイヤーマスク

	LayerMask rayMask = ray.layerMask;				//レイのレイヤーマスク
	LayerMask colMask = collider->GetLayerMask();	//コライダーのレイヤーマスク

	bool rayWantsCol = (rayMask & colBit) != 0;	//レイがコライダーと衝突したいかどうか
	bool colWantsRay = (colMask & rayBit) != 0;	//コライダーがレイと衝突したいかどうか

	return rayWantsCol && colWantsRay;	//互いに衝突したい場合はtrueを返す
}

//レイキャストの衝突状態を更新
void CollisionSystem::UpdateRaycastCollisionState(RaycastSegment& ray)
{
	std::unordered_set<Collider*> prevSet;
	prevSet.reserve(ray.prevHitInfos.size());

	for (auto& p : ray.prevHitInfos)
	{
		if(p.opponent) prevSet.insert(p.opponent);
	}

	std::unordered_set<Collider*> currSet;
	currSet.reserve(ray.currHitInfos.size());

	for (auto& p : ray.currHitInfos)
	{
		if (!p.opponent) continue;
		currSet.insert(p.opponent);

		p.state = (prevSet.find(p.opponent) != prevSet.end())
			? COLLISION_STATE::COLLISION_STAY 
			: COLLISION_STATE::COLLISION_ENTER;
	}

	for (auto& p : ray.prevHitInfos)
	{
		if (!p.opponent) continue;
		if (currSet.find(p.opponent) == currSet.end())
		{
			RaycastHitInfo exitInfo = p;
			exitInfo.state = COLLISION_STATE::COLLISION_EXIT;
			exitInfo.hitDistance = FLT_MAX;
			ray.currHitInfos.push_back(exitInfo);
		}
	}
}

//レイキャストとボックスコライダーの衝突判定
bool CollisionSystem::RaycastBox(const RaycastSegment& ray, Collider* collider, RaycastHitInfo& outHitInfo)
{
	const float EPSILON = 0.0001f; //微小値

	auto obb = CreateOBB(collider, 1.0f); //コライダーのOBB情報取得

	//レイの始点と方向ベクトル取得
	XMVECTOR rayOrigin = XMLoadFloat3(&ray.startPoint);				//レイの始点ベクトル
	XMVECTOR rayDirection = XMVectorSubtract(						//レイの方向ベクトル
		XMLoadFloat3(&ray.endPoint),
		rayOrigin
	);
	float rayLength = XMVectorGetX(XMVector3Length(rayDirection));	//レイの長さ

	if (rayLength < EPSILON) return false; //レイの長さが極端に短い場合、衝突しない

	XMVECTOR directionNorm = XMVectorScale(	//正規化されたレイの方向ベクトル
		rayDirection,
		1.0f / rayLength
	);

	//レイの始点をOBBのローカル座標系に変換
	XMVECTOR relativeOrigin = XMVectorSubtract(rayOrigin, obb.center); 

	//レイの始点からOBBの中心へのベクトル
	float originLocal[3];		//レイの始点のOBBローカル座標系での位置
	float directionLocal[3];	//レイの方向ベクトルのOBBローカル座標系での成分
	for(int i = 0; i < 3; i++)
	{
		originLocal[i] = XMVectorGetX(XMVector3Dot(relativeOrigin, obb.axis[i]));
		directionLocal[i] = XMVectorGetX(XMVector3Dot(directionNorm, obb.axis[i]));
	}

	//スラブ法によるレイとOBBの衝突判定
	const Vector3& halfSizes = obb.halfSizes; //OBBの各軸方向の半分のサイズ
	float half[3] = { halfSizes.x, halfSizes.y, halfSizes.z };

	float tEnter = 0.0f;		//レイの入り口パラメータ
	float tExit = rayLength;	//レイの出口パラメータ

	for (int i = 0; i < 3; i++)
	{
		float origin = originLocal[i];			//レイの始点のOBBローカル座標系での位置
		float direction = directionLocal[i];	//レイの方向ベクトルのOBBローカル座標系での成分
		float halfSize = half[i];				//OBBの各軸方向の半分のサイズ

		if (fabs(direction) < EPSILON)
		{
			//レイがスラブに平行な場合、始点がスラブの範囲内にあるかチェック
			if(origin < -halfSize || origin > halfSize) return false;
		}
		else
		{
			float invD = 1.0f / direction;			//方向成分の逆数
			float t1 = (-halfSize - origin) * invD;	//スラブの負の面との交点パラメータ
			float t2 = (halfSize - origin) * invD;	//スラブの正の面との交点パラメータ

			if (t1 > t2) std::swap(t1, t2);	//交点パラメータの入れ替え
			if (t1 > tEnter) tEnter = t1;	//入り口パラメータの更新
			if (t2 < tExit) tExit = t2;		//出口パラメータの更新

			if (tEnter > tExit) return false;	//入り口パラメータが出口パラメータを超えた場合、衝突しない
		}
	}

	float tHit = (tEnter >= 0.0f) ? tEnter : tExit;		//衝突パラメータ
	if (tHit < 0.0f || tHit > rayLength) return false;	//衝突パラメータがレイの範囲外の場合、衝突しない

	//衝突情報の設定
	//衝突点の計算
	XMVECTOR hitPoint = XMVectorAdd(
		rayOrigin,
		XMVectorScale(
			directionNorm,
			tHit
		)
	);

	//法線の計算
	float hitNormalLocal[3] = { 0.0f, 0.0f, 0.0f }; //衝突面の法線ベクトル(OBBローカル座標系)
	for(int i = 0; i < 3; i++)
	{
		hitNormalLocal[i] = originLocal[i] + directionLocal[i] * tHit;	//衝突点のOBBローカル座標系での位置
	}

	float dx = half[0] - fabsf(hitNormalLocal[0]);	//X軸方向の衝突面までの距離
	float dy = half[1] - fabsf(hitNormalLocal[1]);	//Y軸方向の衝突面までの距離
	float dz = half[2] - fabsf(hitNormalLocal[2]);	//Z軸方向の衝突面までの距離

	int axis = 0;													 //最も近い衝突面の軸インデックス
	if (dy < dx) axis = 1;											//Y軸方向の衝突面が最も近い場合
	if ((axis == 1 && dz < dy) || (axis == 0 && dz < dx)) axis = 2;	//Z軸方向の衝突面が最も近い場合

	float sign = (hitNormalLocal[axis] >= 0.0f) ? 1.0f : -1.0f; //法線の向き

	XMVECTOR noramalWorld = (sign > 0.0f) ? obb.axis[axis] : XMVectorNegate(obb.axis[axis]); //法線ベクトル(ワールド座標系)

	//衝突情報の作成
	outHitInfo.opponent = collider;						//衝突したコライダー
	XMStoreFloat3(&outHitInfo.hitPoint, hitPoint);		//衝突点
	XMStoreFloat3(&outHitInfo.hitNormal, noramalWorld);	//法線ベクトル
	outHitInfo.hitDistance = tHit;						//衝突距離

	return true;
}

//レイキャストと球コライダーの衝突判定
bool CollisionSystem::RaycastSphere(const RaycastSegment& ray, Collider* collider, RaycastHitInfo& outHitInfo)
{
	//レイの始点と方向ベクトル取得
	XMVECTOR rayOrigin = XMLoadFloat3(&ray.startPoint);				//レイの始点ベクトル
	XMVECTOR rayDirection = XMVectorSubtract(						//レイの方向ベクトル
		XMLoadFloat3(&ray.endPoint),
		rayOrigin
	);
	float rayLength = XMVectorGetX(XMVector3Length(rayDirection));	//レイの長さ
	if (rayLength <= 1e-6f) return false; //レイの長さが極端に短い場合、衝突しない

	XMVECTOR directionNorm = XMVectorScale(	//正規化されたレイの方向ベクトル
		rayDirection,
		1.0f / rayLength
	);

	//球コライダー情報取得
	auto sphere = collider->GetCurrentSphereCollider();		//球コライダー情報取得
	XMVECTOR sphereCenter = XMLoadFloat3(&sphere.center);	//球の中心点ベクトル
	float radius = sphere.radius;							//球の半径

	//レイと球の衝突判定
	XMVECTOR m = XMVectorSubtract(rayOrigin, sphereCenter);

	float b = XMVectorGetX(XMVector3Dot(m, directionNorm));
	float c = XMVectorGetX(XMVector3Dot(m, m)) - radius * radius;

	if (c > 0.0f && b > 0.0f)
	{//レイの始点が球の外側にあり、レイが球から離れていく場合
		return false;
	}

	float disc = b * b - c;
	if (disc < 0.0f)
	{//判別式が負の場合、衝突しない
		return false;
	}

	float t = -b - sqrtf(disc);
	if (t < 0.0f)
	{//レイの始点が球の内部にある場合
		t = 0.0f;
	}

	if (t > rayLength)
	{//レイの長さを超える場合、衝突しない
		return false;
	}

	//衝突情報の設定
	//衝突点の計算
	XMVECTOR hitPoint = XMVectorAdd(
		rayOrigin,
		XMVectorScale(
			XMVector3Normalize(rayDirection),
			t
		)
	);

	//法線の計算
	XMVECTOR hitNormal = XMVectorSubtract(hitPoint, sphereCenter);	//法線ベクトルの計算
	hitNormal = XMVector3Normalize(hitNormal);						//法線ベクトルの正規化

	//衝突情報の作成
	RaycastHitInfo hitInfo{};
	hitInfo.opponent = collider;					//衝突したコライダー
	XMStoreFloat3(&hitInfo.hitPoint, hitPoint);		//衝突点
	XMStoreFloat3(&hitInfo.hitNormal, hitNormal);	//法線ベクトル
	hitInfo.hitDistance = t;						//衝突距離
	outHitInfo = hitInfo;							//衝突情報を出力引数に設定

	return true;
}

//レイキャストとカプセルコライダーの衝突判定
bool CollisionSystem::RaycastCapsule(const RaycastSegment& ray, Collider* collider, RaycastHitInfo& outHitInfo)
{
	const float EPSILON = 0.0001f; //微小値

	//レイの始点と方向ベクトル取得
	XMVECTOR rayOrigin = XMLoadFloat3(&ray.startPoint);				//レイの始点ベクトル
	XMVECTOR rayEnd = XMLoadFloat3(&ray.endPoint);					//レイの終点ベクトル
	XMVECTOR rayDirection = XMVectorSubtract(rayEnd, rayOrigin);	//レイの方向ベクトル
	float rayLength = XMVectorGetX(XMVector3Length(rayDirection));	//レイの長さ
	if (rayLength < EPSILON) return false; //レイの長さが極端に短い場合、衝突しない

	//カプセルコライダー情報取得
	auto capsule = collider->GetCurrentCapsuleCollider();

	XMVECTOR outP, outQ; //セグメントP・Q上の最短点
	float distSq = GetMinDistanceSquaredSegmentToSegment(
		rayOrigin, rayEnd,						//セグメントP：レイ
		XMLoadFloat3(&capsule.pointA),			//セグメントQ：カプセルセグメント(端点A)
		XMLoadFloat3(&capsule.pointB),			//セグメントQ：カプセルセグメント(端点B)
		outP, outQ								//各セグメント上の最短点
	);

	float radius = capsule.radius; //カプセルの半径
	if (distSq > radius * radius) return false; //衝突なし

	//衝突情報の設定
	//衝突点の計算
	Vector3 hitPos;
	XMStoreFloat3(&hitPos, outP); //レイ上の最短点を衝突点とする

	//法線の計算
	XMVECTOR diff = XMVectorSubtract(outP, outQ);			//最短点同士の差ベクトル
	float lenSq = XMVectorGetX(XMVector3LengthSq(diff));	//最短距離の二乗

	XMVECTOR hitNormal;
	if(lenSq > EPSILON * EPSILON)
	{//最短距離がほぼ0でない場合
		hitNormal = XMVector3Normalize(diff); //法線ベクトルの計算
	}
	else
	{//最短距離がほぼ0の場合の処理
		hitNormal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); //適当な法線ベクトル
	}

	//衝突点から法線方向に半径分移動して、カプセル表面上の点を計算
	float hitDistance = XMVectorGetX(XMVector3Length(XMVectorSubtract(outP, rayOrigin))); //衝突距離

	//衝突情報の作成
	outHitInfo.opponent = collider;						//衝突したコライダー
	XMStoreFloat3(&outHitInfo.hitPoint, outP);			//衝突点
	XMStoreFloat3(&outHitInfo.hitNormal, hitNormal);	//法線ベクトル
	outHitInfo.hitDistance = hitDistance;				//衝突距離

	return true;
}