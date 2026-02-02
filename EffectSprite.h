#pragma once
#include "EffectBase.h"

//エフェクトスプライトクラス
class EffectSprite : public EffectBase
{
public:
	EffectSprite(
		EFFECT_TYPE type = EFFECT_TYPE::NONE,	//Effect type
		DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f },				//Center
		DirectX::XMFLOAT2 size = { 1.0f, 1.0f },					//Size
		DirectX::XMFLOAT4 color =				//Color RGBA
		DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		float lifeTime = 1.0f,					//Life time
		bool isActive = true,					//Active flag
		MESH_TYPE meshType =
		MESH_TYPE::QUAD				//Mesh type
	)
		: EffectBase(
			center,
			size,
			color,
			lifeTime,
			isActive,
			meshType
		) {
	};
	~EffectSprite() {};

	EFFECT_TYPE GetType() const { return m_type; }		//エフェクトタイプ取得
	void SetType(EFFECT_TYPE type) { m_type = type; }	//エフェクトタイプ設定

private:
	EFFECT_TYPE m_type = EFFECT_TYPE::NONE;	//エフェクトタイプ

private:
	void UpdateOverride() override {};	//更新オーバーライド関数
};