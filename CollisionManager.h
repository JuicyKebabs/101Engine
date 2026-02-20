#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include "Collider.h"
#include "SharedStruct.h"
#include "RenderData.h"
#include "Context.h"
#include "CollisionData.h"

//前方宣言
class Renderer;
class TextureManager;
class MeshManager;

//衝突ペア構造体
struct CollisionPair
{
	Collider* colliderA; // コライダーA
	Collider* colliderB; // コライダーB
};

//OBB構造体
struct OBB
{
	DirectX::XMVECTOR center;		//中心点
	DirectX::XMVECTOR axis[3];		//各軸の方向ベクトル(正規化済みワールド軸)
	DirectX::XMFLOAT3 halfSizes;	//各軸方向の半分のサイズ
};

//球セグメント構造体
struct SphereSegment
{
	DirectX::XMVECTOR center;	//中心点
	float radius;				//半径
};

//カプセルセグメント構造体
struct CapsuleSegment
{
	DirectX::XMVECTOR pointA;	//端点A
	DirectX::XMVECTOR pointB;	//端点B
	float radius;				//半径
};

//衝突時のパラメータ
struct ContactResult
{
	bool isCollided = false;	//衝突しているかどうか
	DirectX::XMVECTOR point{};	//接触点
	DirectX::XMVECTOR normal{};	//接触法線
	float depth = 0.0f;			//貫通深度
};

// 衝突管理クラス
class CollisionManager
{
public:
	static constexpr DirectX::XMFLOAT4 DRAW_COLOR_DEFAULT = { 0.0f, 1.0f, 0.0f, 0.1f };		//描画時のデフォルトカラー
	static constexpr DirectX::XMFLOAT4 DRAW_COLOR_DETECTED = { 1.0f, 0.0f, 0.0f, 0.1f };	//描画時の衝突検知時カラー

public:
	const wchar_t* texPath = L"asset/texture/white.png";

public:
	CollisionManager();		//コンストラクタ
	~CollisionManager();	//デストラクタ

	//メイン関数
	void Initialize(EngineContext& context);
	void Draw(Renderer& renderer);		//描画
	void CheckColliders();
	void SubmitDraw(
		Renderer& renderer,			//シーンの参照
		Collider& collider,			//コライダー配列
		WorldRenderModel& infos	//描画モデル
	);

	//衝突判定処理
	void CheckCollisions();			//衝突判定
	void CheckCollisionStates();	//衝突状態チェック

	//コライダー配列の操作
	void RegisterCollider(Collider* collider);	//コライダー登録
	void ClearColliders();						//コライダークリア

	void CreateColliderRenderInfo(	//コライダー描画情報作成
		TextureManager& textureManager,	//テクスチャ管理クラスの参照
		MeshManager& meshManager		//メッシュ管理クラスの参照
	);

	//レイキャスト関数
	void RaycastSegmentQuery(
		RaycastSegment& ray	//レイ情報
	);

private:
	std::vector<Collider*> m_pCollidersList;				//コライダー配列
	std::vector<CollisionPair> m_pNarrowPhaseColliders;		//ナローフェーズ用コライダー配列
	std::vector<CollisionPair> m_currentCollisionPairs;		//今回の衝突ペア配列
	std::vector<CollisionPair> m_previousCollisionPairs;	//前回の衝突ペア配列

	WorldRenderModel m_colliderRenderModelBox;		//ボックスコライダー描画情報
	WorldRenderModel m_colliderRenderModelSphere;	//球コライダー描画情報
	WorldRenderModel m_colliderRenderModelCapsule;	//カプセルコライダー描画情報

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