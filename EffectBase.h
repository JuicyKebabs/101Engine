#pragma once
#include "EffectData.h"
#include "SharedStruct.h"

//前方宣言
class TextureManager;
class MeshManager;

//エフェクト基底クラス
class EffectBase
{
public:
	EffectBase(
		EFFECT_TYPE type = EFFECT_TYPE::NONE,									//エフェクトタイプ
		DirectX::XMFLOAT3 center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),			//座標
		DirectX::XMFLOAT3 size = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f),			//サイズ
		DirectX::XMFLOAT4 color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),	//色RGBA
		float lifeTime = 1.0f,													//寿命
		bool isActive = false,													//アクティブフラグ
		MESH_TYPE meshType = QUAD							//メッシュタイプ
	);	//コンストラクタ
	virtual ~EffectBase() = default;	//デストラクタ

	void Update();	//更新
	void Reset();	//リセット
	void Destroy();	//破棄

	//ゲッター
	EFFECT_TYPE GetType() const { return m_type; }				//エフェクトタイプ取得
	DirectX::XMFLOAT3 GetCenter() const { return m_center; }	//座標取得
	DirectX::XMFLOAT3 GetSize() const { return m_size; }		//サイズ取得
	DirectX::XMFLOAT4 GetColor() const { return m_color; }		//色RGBA取得
	float GetLifeTime() const { return m_lifeTime; }			//寿命取得
	bool IsActive() const { return m_isActive; }				//アクティブフラグ取得
	bool IsChase() const { return m_isChase; }				//追従フラグ取得
	DirectX::XMFLOAT3* GetChaseTarget() const { return m_chaseTarget; }	//追従ターゲット座標取得
	MESH_TYPE GetMeshType() const { return m_meshType; }	//メッシュタイプ取得
	const TexSplitInfo& GetTexSplitInfo() const { return m_texSplitInfo; }	//テクスチャ分割情報構造体取得関数

	//セッター
	void SetType(EFFECT_TYPE type) { m_type = type; }			//エフェクトタイプ設定
	void SetCenter(DirectX::XMFLOAT3 center) { m_center = center; }	//座標設定
	void SetSize(DirectX::XMFLOAT3 size) { m_size = size; }			//サイズ設定
	void SetColor(DirectX::XMFLOAT4 color) { m_color = color; }		//色RGBA設定
	void SetLifeTime(float lifeTime) { m_lifeTime = lifeTime; }		//寿命設定
	void SetActive(bool isActive) { m_isActive = isActive; }		//アクティブフラグ設定
	void SetChase(bool isChase, DirectX::XMFLOAT3* chaseTarget = nullptr)	//追従フラグ設定
	 {
		 m_isChase = isChase;
		 m_chaseTarget = chaseTarget;
	}
	void SetTexSplitInfo(const TexSplitInfo& texSplitInfo) { m_texSplitInfo = texSplitInfo; }	//テクスチャ分割情報構造体設定関数

protected:	//非公開メンバ変数
	EFFECT_TYPE m_type = EFFECT_TYPE::NONE;	//エフェクトタイプ
	DirectX::XMFLOAT3 m_center{};			//座標
	DirectX::XMFLOAT3 m_size{};				//サイズ
	DirectX::XMFLOAT4 m_color{};			//色RGBA
	float m_lifeTime = 0.0f;				//寿命
	float m_elapsedTime = 0.0f;				//経過時間
	bool m_isActive = false;				//アクティブフラグ

	bool m_isChase = false;						//追従フラグ
	DirectX::XMFLOAT3* m_chaseTarget = nullptr;	//追従ターゲット座標

	MESH_TYPE m_meshType = QUAD;	//メッシュタイプ
	TexSplitInfo m_texSplitInfo {};	//テクスチャ分割情報構造体

	void UpdateLifeTime();		//寿命更新
	void UpdateAnimation();		//アニメーション更新
	void UpdatePositionChase();	//位置追従更新
};