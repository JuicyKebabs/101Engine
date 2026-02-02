#include "SharedStruct.h"
#include <algorithm>
#include "Actor.h"

using namespace DirectX;

//頂点データの入力要素配列の定義
const D3D12_INPUT_ELEMENT_DESC Vertex::InputElements[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

//頂点データの入力レイアウトの定義
const D3D12_INPUT_LAYOUT_DESC Vertex::InputLayout =
{
	Vertex::InputElements,		// 入力要素配列のポインタ
	Vertex::InputLayoutCount	// 入力要素の数
};

//コリジョンレイヤーをビットマスクに変換する関数
CollisionData::LayerMask CollisionData::LayerToBit(COLLISION_LAYER layer)
{
	return static_cast<LayerMask>(1 << static_cast<uint32_t>(layer));
}

//コリジョンレイヤーからレイヤーマスクを取得する関数
CollisionData::LayerMask CollisionData::GetLayerMask(COLLISION_LAYER layer)
{
	switch (layer)
	{//コリジョンレイヤーごとに当たり判定を行うレイヤーマスクを設定
	case CollisionData::COLLISION_LAYER::DEFAULT:
		break;
	case CollisionData::COLLISION_LAYER::PLAYER:
		return MakeMask({
			COLLISION_LAYER::ENEMY,				//エネミーレイヤー
			COLLISION_LAYER::WALL,				//壁レイヤー
			COLLISION_LAYER::ENEMY_BULLET		//エネミーブレットレイヤー
			});
		break;

	case CollisionData::COLLISION_LAYER::ENEMY:
		return MakeMask({
			COLLISION_LAYER::PLAYER,			//プレイヤーレイヤー
			COLLISION_LAYER::PLAYER_BULLET,		//プレイヤーブレットレイヤー
			COLLISION_LAYER::PLAYER_RAY			//プレイヤーレイキャストレイヤー
			});
		break;

	case CollisionData::COLLISION_LAYER::WALL:
		return MakeMask({
			COLLISION_LAYER::PLAYER,			//プレイヤーレイヤー
			COLLISION_LAYER::PLAYER_BULLET,		//プレイヤーブレットレイヤー
			COLLISION_LAYER::PLAYER_RAY			//プレイヤーレイキャストレイヤー
			});
		break;

	case CollisionData::COLLISION_LAYER::PLAYER_BULLET:
		return MakeMask({
			COLLISION_LAYER::ENEMY,				//エネミーレイヤー
			COLLISION_LAYER::WALL				//壁レイヤー
			});
		break;

	case CollisionData::COLLISION_LAYER::PLAYER_RAY:
		return MakeMask({
			COLLISION_LAYER::ENEMY,				//エネミーレイヤー
			COLLISION_LAYER::WALL				//壁レイヤー
			});
		break;

	case CollisionData::COLLISION_LAYER::ENEMY_BULLET:
		return MakeMask({
			COLLISION_LAYER::PLAYER,			//プレイヤーレイヤー
			});
		break;

	case CollisionData::COLLISION_LAYER::MAX_LAYER:
		return 0;
		break;
	default:
		break;
	}
}

//複数のコリジョンレイヤーからレイヤーマスクを作成する関数
CollisionData::LayerMask CollisionData::MakeMask(std::initializer_list<COLLISION_LAYER> layers)
{
	LayerMask mask = 0;	//レイヤーマスク
	for (auto layer : layers)
	{
		mask |= LayerToBit(layer);	//ビットマスクを合成
	}
	return mask;	//レイヤーマスクを返す
}

//貫入深さから押し出しベクトルを取得する関数
DirectX::XMFLOAT3 CollisionData::GetPushOutVector(
	std::vector<CollisionData::ObjectCollisionInfo>& infos,	//衝突情報配列
	const std::initializer_list<OBJECT_TAG>& tagList	//押し出しベクトルを計算する対象のタグリスト
)
{
	using namespace DirectX;

	XMFLOAT3 total{ 0,0,0 };	//最大押し出しベクトル
	float epsilon = 0.0001f;	//誤差許容値

	std::vector<CollisionData::ObjectCollisionInfo*> cands;	//衝突情報配列をループ

	for (auto& info : infos)
	{
		//衝突終了は無視
		if (info.state == CollisionData::COLLISION_STATE::COLLISION_EXIT) continue;

		OBJECT_TAG opponentTag = info.opponent->GetTag();	//衝突相手のタグ取得

		//衝突相手のタグがリストに含まれているか確認
		if (std::find(tagList.begin(), tagList.end(), opponentTag) == tagList.end()) continue;

		cands.push_back(&info);	//候補リストに追加
	}

	auto begin = cands.begin();
	auto end = cands.end();

	//貫入深さの大きい順にソート
	std::sort(cands.begin(), cands.end(),
		[](const CollisionData::ObjectCollisionInfo* a, const CollisionData::ObjectCollisionInfo* b)
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
