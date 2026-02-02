#include "Actor.h"
#include "d3dx12.h"
#include <DirectXMath.h>
#include "Engine.h"
#include "Renderer.h"
#include "RenderData.h"
#include "CollisionManager.h"

using namespace DirectX;
using namespace CollisionData;

//コンストラクタ
Actor::Actor(
	MESH_TYPE meshType,
	XMFLOAT3 position,
	XMFLOAT3 rotation,
	XMFLOAT3 scale,
	XMFLOAT3 velocity,
	bool isActive, 
	OBJECT_TAG tag,
	COLLISION_LAYER layer,
	XMFLOAT3 colliderSetScale,
	XMFLOAT3 colliderSetOffsetPosition,
	XMFLOAT3 colliderSetOffsetRotation
) : 
	m_meshType(meshType),
	m_position(position), 
	m_rotation(rotation), 
	m_scale(scale), 
	m_velocity(velocity), 
	m_isActive(isActive), 
	m_tag(tag)
{
	m_pColliderSet = new ColliderSet(
		this,
		tag,
		position,
		colliderSetScale,
		rotation,
		layer,
		true,
		colliderSetOffsetPosition,
		colliderSetOffsetRotation
	);

	m_nodeAnimatorSet.pNodeAnimator = new NodeAnimator();
}

//デストラクタ
Actor::~Actor()
{
	//コライダーの破棄
	if (m_pColliderSet)
	{
		m_pColliderSet->SetDeleteFlag(true);
		m_pColliderSet = nullptr;
	}
}

void Actor::Update()
{
	m_passedFrames++;
	UpdateOverride();
	m_pColliderSet->Update();
	UpdateAnimation();
}

void Actor::SubmitDraws(Renderer& renderer)
{
	//アクティブなオブジェクトの描画要求をシーンに提出
	if (IsActive() && IsDrawn())
	{//アクティブかつ描画フラグが立っている場合

		auto info = GetRenderModel();	//描画情報構造体配列取得
		std::vector<WorldRenderInfo> submitInfos;	//Rendererへの提出用描画情報構造体配列
		submitInfos.reserve(info.size());		//容量確保

		if (GetMeshType() == MESH_TYPE::CAPSULE)
		{//カプセルメッシュの場合(複数メッシュに分かれているため個別に処理)
			CapsuleVisualDesc desc{};	//カプセルメッシュの記述データ
			//カプセルメッシュの記述データ設定
			AppendCapsuleRenderInfos(
				desc,			//カプセル描画情報記述子
				GetPosition(),	//位置
				GetScale(),		//スケール
				GetRotation(),	//回転Euler角
				GetColor(),		//色
				info,			//入力元描画情報配列
				submitInfos		//出力先描画情報配列
			);
		}
		else
		{//それ以外のメッシュの場合
			//描画情報構造体配列をそのまま提出用配列にコピー
			for (auto& i : info)
			{
				submitInfos.push_back(i);
			}

			//ワールド行列と色を設定
			for (int i = 0; i < submitInfos.size(); i++)
			{
				submitInfos[i].world = GetWorldMatrix();

				//ノードアニメーションがある場合、ワールド行列を上書き
				if(m_nodeAnimatorSet.isAnimLoaded && m_nodeAnimatorSet.isAnimPlaying)
				{
					const auto nodeWorld = m_nodeAnimatorSet.pNodeAnimator->GetMeshGlobalTransform(i);
					submitInfos[i].world = XMMatrixMultiply(
						AiMatrix4x4ToXMMatrix(nodeWorld),
						GetWorldMatrix()
					);
				}

				submitInfos[i].common.color =
				{
					GetColor().x * submitInfos[i].common.color.x,
					GetColor().y * submitInfos[i].common.color.y,
					GetColor().z * submitInfos[i].common.color.z,
					GetColor().w * submitInfos[i].common.color.w
				};
			}
		}

		//共通要素の設定
		for (int i = 0; i < submitInfos.size(); i++)
		{
			submitInfos[i].position = GetPosition();
			submitInfos[i].scale = GetScale();
			submitInfos[i].common.blendMode = info[i].common.blendMode;
			submitInfos[i].common.uvRect = SplitSprite(GetTexSplitInfo());
			submitInfos[i].billboardType = info[i].billboardType;
		}

		renderer.SubmitToWorldList(submitInfos);	//Rendererに描画情報提出
	}

}

//衝突解決
void Actor::ResolveCollisions()
{
	m_pColliderSet->BuildObjectCollisionInfos();	//衝突情報を収集
	ResolveCollisionsOverride();					//衝突解決(固有処理用、派生クラスでオーバーライド)
	m_pColliderSet->Update();						//コライダーの更新
	ClearCollisionInfos();							//衝突情報のクリア
}

//衝突情報の追加
void Collider::AddCollisionInfo(const CollisionInfo& info)
{
	m_collisionInfos.push_back(info);
}

//衝突情報のクリア
void Collider::ClearInfos()
{
	m_collisionInfos.clear();
}

//衝突情報のクリア
void Actor::ClearCollisionInfos()
{
	m_pColliderSet->ClearCollisionInfos();
}

//ワールド行列の取得
const DirectX::XMMATRIX Actor::GetWorldMatrix() const
{
	XMMATRIX T = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(m_rotation.x),
		XMConvertToRadians(m_rotation.y),
		XMConvertToRadians(m_rotation.z));
	XMMATRIX S = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	return S * R * T;
}

//位置の取得
const DirectX::XMFLOAT3 Actor::GetPosition() const
{
	return m_position;
}

//回転の取得
const DirectX::XMFLOAT3 Actor::GetRotation() const
{
	return m_rotation;
}

//スケールの取得
const DirectX::XMFLOAT3 Actor::GetScale() const
{
	return m_scale;
}

//色RGBAの取得
const DirectX::XMFLOAT4 Actor::GetColor() const
{
	return m_color;
}

//getting velocity
const DirectX::XMFLOAT3 Actor::GetVelocity() const
{
	return m_velocity;
}

//アクティブかどうかを取得
const bool Actor::IsActive() const
{
	return m_isActive;
}

//描画フラグの取得
const bool Actor::IsDrawn() const
{
	return m_isDrawn;
}

//位置の設定
void Actor::SetPosition(DirectX::XMFLOAT3 position)
{
	m_position = position;
}

//回転の設定
void Actor::SetRotation(DirectX::XMFLOAT3 rotation)
{
	m_rotation = rotation;
}

//スケールの設定
void Actor::SetScale(DirectX::XMFLOAT3 scale)
{
	m_scale = scale;
}

//色RGBAの設定
void Actor::SetColor(DirectX::XMFLOAT4 color)
{
	m_color = color;
}

//setting velocity
void Actor::SetVelocity(DirectX::XMFLOAT3 velocity)
{
	m_velocity = velocity;
}

//アクティブフラグの設定
void Actor::SetActive(bool isActive)
{
	m_isActive = isActive;
	m_pColliderSet->SetActive(isActive);
}

//描画フラグの設定
void Actor::SetDrawn(bool isDrawn)
{
	m_isDrawn = isDrawn;
}

//テクスチャ分割情報構造体の設定
void Actor::SetTexSplitInfo(TexSplitInfo info)
{
	m_texSplitInfo = info;
}

// Get mesh type
MESH_TYPE Actor::GetMeshType() const
{
	return m_meshType;
}

// Set render model
void Actor::SetRenderModel(const WorldRenderModel& renderModel)
{
	m_renderModel = renderModel;
}

//ノードアニメーションセットの設定
void Actor::SetNodeAnimatorSet(const NodeAnimatorSet& nodeAnimatorSet)
{
	m_nodeAnimatorSet = nodeAnimatorSet;
}

//アニメーション更新
void Actor::UpdateAnimation()
{
	if (m_texSplitInfo.total <= 1 || m_texSplitInfo.updateRate <= 0) return;

	m_texSplitInfo.frameCount++;	//フレームカウントをインクリメント

	//更新頻度に達したらインデックスを更新
	if (m_texSplitInfo.frameCount >= m_texSplitInfo.updateRate)
	{
		m_texSplitInfo.frameCount = 0;	//フレームカウントリセット
		m_texSplitInfo.index++;			//インデックスをインクリメント

		//インデックスが総数を超えたらリセット
		if (m_texSplitInfo.index >= m_texSplitInfo.total)
		{
			m_texSplitInfo.index = 0;
		}
	}
}

//コライダーの取得
ColliderSet* Actor::GetColliderSet() const
{
	return m_pColliderSet;
}

//オブジェクトタグの取得
OBJECT_TAG Actor::GetTag() const
{
	return m_tag;
}

//テクスチャ分割情報構造体取得関数
const TexSplitInfo& Actor::GetTexSplitInfo() const
{
	return m_texSplitInfo;
}

// Get render model
WorldRenderModel Actor::GetRenderModel()
{
	return m_renderModel;
}

//ノードアニメーションセットの取得
NodeAnimatorSet* Actor::GetNodeAnimatorSet()
{
	return &m_nodeAnimatorSet;
}
