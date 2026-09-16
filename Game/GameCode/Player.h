#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/Component/Collider.h"
#include "Engine/Component/Camera.h"
#include "Engine/ActorImprint/ActorImprint.h"
#include "Engine/Resource/AssetReference.h"

class Player : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
    static std::optional<TypeMetadata> BuildMetadata();

	void SetCanMove(bool canMove) { m_canMove = canMove; }	// プレイヤーの移動可否を設定
	bool CanMove() const { return m_canMove; }				// プレイヤーが移動可能かどうかを取得

private:
	float m_moveSpeed = 5.0f;		// 移動スピード
	float m_moveSlowSpeed = 2.5f;   // 移動スピード
	float m_bulletSpeed = 2.0f; // 弾の移動スピード
	float m_sensitivity = 0.1f; // 視点操作のマウス感度

	AssetReference<ActorImprint> m_bulletImprintRef; // 弾のアクターインプリントへの参照
	AssetReference<AudioAsset> m_fireSoundRef; // 射撃SEへの参照

	Vector3 m_velocity = Vector3::Zero(); // プレイヤーの速度ベクトル

	Collider* m_collider = nullptr; // コライダーコンポーネントへのポインタ
	Camera* m_camera = nullptr;		// カメラコンポーネントへのポインタ

	float m_defaultFOV = 90.0f;			// デフォルトのFOV
	float m_aimingFOV = 45.0f;			// エイム時のFOV
	float m_fovSwitchingSpeed = 0.5f;	// FOV切り替えのスピード
	bool m_isAiming = false;			// エイム中かどうかのフラグ
	float m_fovSwithingTimer = 0.0f;	// FOV切り替えのタイマー

	bool m_canMove = true;	// プレイヤーが移動できるかどうかのフラグ
};

