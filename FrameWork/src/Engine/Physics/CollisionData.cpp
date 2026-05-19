#include "CollisionData.h"
#include "Engine/Actor/Actor.h"
#include <algorithm>

using namespace DirectX;

//コリジョンレイヤーをビットマスクに変換する関数
LayerMask LayerToBit(CollisionLayer layer)
{
	return static_cast<LayerMask>(1 << static_cast<uint32_t>(layer));
}

//コリジョンレイヤーからレイヤーマスクを取得する関数
LayerMask MakeLayerMask(CollisionLayer layer)
{
	switch (layer)
	{//コリジョンレイヤーごとに当たり判定を行うレイヤーマスクを設定
	case CollisionLayer::Default:
		return MakeMask({
			CollisionLayer::Default,		//デフォルトレイヤー
			CollisionLayer::PLAYER,			//プレイヤーレイヤー
			CollisionLayer::ENEMY,			//エネミーレイヤー
			CollisionLayer::WALL,			//壁レイヤー
			CollisionLayer::PLAYER_BULLET,	//プレイヤーブレットレイヤー
			CollisionLayer::PLAYER_RAY,		//プレイヤーレイキャストレイヤー
			CollisionLayer::ENEMY_BULLET	//エネミーブレットレイヤー
			});
		break;
	case CollisionLayer::PLAYER:
		return MakeMask({
			CollisionLayer::ENEMY,				//エネミーレイヤー
			CollisionLayer::WALL,				//壁レイヤー
			CollisionLayer::ENEMY_BULLET		//エネミーブレットレイヤー
			});
		break;

	case CollisionLayer::ENEMY:
		return MakeMask({
			CollisionLayer::PLAYER,			//プレイヤーレイヤー
			CollisionLayer::PLAYER_BULLET,		//プレイヤーブレットレイヤー
			CollisionLayer::PLAYER_RAY			//プレイヤーレイキャストレイヤー
			});
		break;

	case CollisionLayer::WALL:
		return MakeMask({
			CollisionLayer::PLAYER,			//プレイヤーレイヤー
			CollisionLayer::PLAYER_BULLET,		//プレイヤーブレットレイヤー
			CollisionLayer::PLAYER_RAY			//プレイヤーレイキャストレイヤー
			});
		break;

	case CollisionLayer::PLAYER_BULLET:
		return MakeMask({
			CollisionLayer::ENEMY,				//エネミーレイヤー
			CollisionLayer::WALL				//壁レイヤー
			});
		break;

	case CollisionLayer::PLAYER_RAY:
		return MakeMask({
			CollisionLayer::ENEMY,				//エネミーレイヤー
			CollisionLayer::WALL				//壁レイヤー
			});
		break;

	case CollisionLayer::ENEMY_BULLET:
		return MakeMask({
			CollisionLayer::PLAYER,			//プレイヤーレイヤー
			});
		break;

	case CollisionLayer::MAX_LAYER:
		return 0;
	default:
		return 0;
		break;
	}
}

//複数のコリジョンレイヤーからレイヤーマスクを作成する関数
LayerMask MakeMask(std::initializer_list<CollisionLayer> layers)
{
	LayerMask mask = 0;	//レイヤーマスク
	for (auto layer : layers)
	{
		mask |= LayerToBit(layer);	//ビットマスクを合成
	}
	return mask;	//レイヤーマスクを返す
}

//貫入深さから押し出しベクトルを取得する関数
DirectX::XMFLOAT3 GetPushOutVector(
	std::vector<ObjectCollisionInfo>& infos,	//衝突情報配列
	const std::initializer_list<TagId>& tagList	//押し出しベクトルを計算する対象のタグリスト
)
{
	using namespace DirectX;

	XMFLOAT3 total{ 0,0,0 };	//最大押し出しベクトル
	float epsilon = 0.0001f;	//誤差許容値

	std::vector<ObjectCollisionInfo*> cands;	//衝突情報配列をループ

	for (auto& info : infos)
	{
		//衝突終了は無視
		if (info.state == COLLISION_STATE::COLLISION_EXIT) continue;

		TagId opponentTag = info.opponent->GetTag();	//衝突相手のタグ取得

		//衝突相手のタグがリストに含まれているか確認
		if (std::find(tagList.begin(), tagList.end(), opponentTag) == tagList.end()) continue;

		cands.push_back(&info);	//候補リストに追加
	}

	auto begin = cands.begin();
	auto end = cands.end();

	//貫入深さの大きい順にソート
	std::sort(cands.begin(), cands.end(),
		[](const ObjectCollisionInfo* a, const ObjectCollisionInfo* b)
		{
			return LengthXMF3(a->penetrationDepth) > LengthXMF3(b->penetrationDepth);
		}
	);

	const int REPEAT_MAX = 5;	//最大繰り返し回数

	for (int i = 0; i < REPEAT_MAX; i++)
	{
		bool any = false;	//押し出しが発生したかどうか

		for (auto pInfo : cands)
		{
			XMFLOAT3 mtv =
			{
				-pInfo->penetrationDepth.x,
				-pInfo->penetrationDepth.y,
				-pInfo->penetrationDepth.z
			};

			float depth = LengthXMF3(mtv);
			if (depth < epsilon)
				continue;

			// 押し出し方向だけ取り出す
			XMFLOAT3 dir = Normalize(mtv);   // 単位ベクトル

			float resolved = (std::max)(0.0f, Dot(total, dir));	//既に押し出された分
			float remain = depth - resolved;					//残りの押し出し分
			if (remain > epsilon)
			{//押し出しが発生する場合
				//押し出しベクトルの加算
				total.x += dir.x * remain;
				total.y += dir.y * remain;
				total.z += dir.z * remain;
				any = true;	//押し出しが発生したフラグを立てる
			}
		}

		if (!any) break;	//押し出しが発生しなかったら終了
	}

	return total;	//押し出しベクトルを返す
}
