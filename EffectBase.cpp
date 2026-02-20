#include "EffectBase.h"

//コンストラクタ
EffectBase::EffectBase(EFFECT_TYPE type, DirectX::XMFLOAT3 center, DirectX::XMFLOAT3 size, DirectX::XMFLOAT4 color, float lifeTime, bool isActive, MESH_TYPE meshType)
	: m_type(type), m_center(center), m_size(size), m_color(color), m_lifeTime(lifeTime), m_isActive(isActive), m_meshType(meshType)
{
}

//更新
void EffectBase::Update()
{
	//寿命更新
	UpdateLifeTime();

	//アニメーション更新
	UpdateAnimation();

	//位置追従更新
	UpdatePositionChase();
}

//リセット
void EffectBase::Reset()
{
	m_elapsedTime = 0.0f;
	m_isActive = true;
}

//破棄
void EffectBase::Destroy()
{
	m_elapsedTime = 0.0f;
	m_isActive = false;
	m_isChase = false;
	m_chaseTarget = nullptr;
}

//寿命更新
void EffectBase::UpdateLifeTime()
{
	if (m_lifeTime <= -1.0f) return; //無限寿命の場合は処理しない

	if (m_elapsedTime <= m_lifeTime)
	{
		m_elapsedTime++; //経過時間をインクリメント

		if (m_elapsedTime > m_lifeTime)
		{
			Destroy(); // 寿命が尽きたら破棄
		}
	}
}

//アニメーション更新
void EffectBase::UpdateAnimation()
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

//位置追従更新
void EffectBase::UpdatePositionChase()
{
	if (m_isChase && m_chaseTarget)
	{
		m_center = *m_chaseTarget;
	}
}
