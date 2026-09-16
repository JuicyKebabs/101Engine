#include "Player.h"
#include "Bullet.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Input/Inputmanager.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/Transform.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Time/Time.h"
#include "Engine/Core/Context/Context.h"
#include "Engine/ActorImprint/ActorImprintSystem.h"
#include "Engine/Actor/ActorTag.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Audio/AudioManager.h"

REGISTER_GAME_COMPONENT(Player)

std::optional<TypeMetadata> Player::BuildMetadata()
{
    TypeMetadataBuilder<Player> builder("Player");

    /*---- Describe the property registration process ----*/
    // Example: register a private member declared in Player.
    // builder.Property("speed", &Player::m_speed);

	builder.Property("moveSpeed", &Player::m_moveSpeed).Optional();
	builder.Property("moveSlowSpeed", &Player::m_moveSlowSpeed).Optional();
	builder.Property("bulletImprint", &Player::m_bulletImprintRef).Optional();
	builder.Property("fireSound", &Player::m_fireSoundRef).Optional();
	builder.Property("bulletSpeed", &Player::m_bulletSpeed).Optional();
	builder.Property("sensitivity", &Player::m_sensitivity).Optional();
	builder.Property("defaultFOV", &Player::m_defaultFOV).Optional();
	builder.Property("aimingFOV", &Player::m_aimingFOV).Optional();
	builder.Property("fovSwitchingSpeed", &Player::m_fovSwitchingSpeed).Optional();

    return builder.Build();
}

void Player::Start()
{
	m_collider = GetOwner()->GetComponentByClass<Collider>();

	Actor* owner = GetOwner();
	std::vector<Actor*> children = owner ? owner->GetChildren() : std::vector<Actor*>();

	// メインカメラアクターを子アクターから探す
	for (Actor* child : children)
	{
		if (child && child->GetTag() == TagRegistry::Get().GetId("MainCamera"))
		{
			m_camera = child->GetComponentByClass<Camera>();
			// 見つかった場合、カメラアクターを取得して処理する
			DBG("Found MainCamera actor: %s", child->GetName().c_str());
			break;
		}
	}
}

void Player::PreUpdate() {}
void Player::Update() 
{	
	if (!m_canMove)
	{
		return; // プレイヤーが移動できない場合は処理をスキップ
	}

	auto inputInfo = InputManager::GetInstance().GetInputInfo();
	auto deltaTime = TimeManager::GetInstance().GetDeltaTime();

	// 前方向を取得する
	auto owner = GetOwner();
	auto transform = owner ? owner->GetComponentByClass<Transform>() : nullptr;

	//-----------------------
	// プレイヤーの視点操作
	//-----------------------
	if (m_camera)
	{
		if (inputInfo.mouse.right.trigger)
		{
			// エイム中フラグの切り替え
			m_isAiming = !m_isAiming;
			m_fovSwithingTimer = 0.0f; // タイマーをリセット
		}
		
		const float aimFOV = DegToRad(m_aimingFOV);						// エイム時の視野角
		const float normalFOV = DegToRad(m_defaultFOV);					// 通常時の視野角
		const float fovSwitchDuration = m_fovSwitchingSpeed;	// 視野角切り替えの時間（秒）

		float newFOV = m_camera->GetCameraLens().fov; // 現在の視野角を初期値として設定

		if (m_isAiming)
		{
			//線形補間で視野角を縮小
			newFOV = Lerp(normalFOV, aimFOV, m_fovSwithingTimer / fovSwitchDuration);
		}
		else
		{
			//線形補間で視野角を拡大
			newFOV = Lerp(aimFOV, normalFOV, m_fovSwithingTimer / fovSwitchDuration);
		}

		m_camera->SetCameraLens({ newFOV, m_camera->GetCameraLens().width, m_camera->GetCameraLens().height, m_camera->GetCameraLens().nearZ, m_camera->GetCameraLens().farZ, m_camera->GetCameraLens().projectionType });

		m_fovSwithingTimer += deltaTime;
		m_fovSwithingTimer = std::clamp(m_fovSwithingTimer, 0.0f, fovSwitchDuration); // タイマーを制限
	}

	if (transform)
	{
		// マウスの移動量を取得
		Vector3 mouseDelta = Vector3(static_cast<float>(inputInfo.mouse.lookDelta.x), static_cast<float>(inputInfo.mouse.lookDelta.y), 0.0f);

		float yaw = mouseDelta.x * m_sensitivity;		// 水平方向の回転量
		float pitch = mouseDelta.y * m_sensitivity;	// 垂直方向の回転量（マウスの上下移動に応じて反転）

		pitch = std::clamp(pitch, -89.0f, 89.0f); // 垂直方向の回転を制限（90度以上は回転できないようにする)

		Quaternion yawRotation = Quaternion::CreateFromAxisAngle(Vector3::Up(), DegToRad(yaw));			// Y軸回転
		Quaternion pitchRotation = Quaternion::CreateFromAxisAngle(Vector3::Right(), DegToRad(pitch));	// X軸回転
		Quaternion newRotation = yawRotation * transform->GetLocalRotationQuat() * pitchRotation;		// 回転の合成
		transform->SetLocalRotationQuat(newRotation);
	}

	//-----------------------
	// 弾の発射
	//-----------------------
	Vector3 forwardDirection = transform ? transform->GetLocalForward() : Vector3::Forward();
	forwardDirection = forwardDirection.Normalized();

	if (inputInfo.mouse.left.trigger)
	{
		// 左クリックが押されたときの処理
		if (transform)
		{
			auto system = GetEngineContext()->pActorImprintSystem;
			if (system)
			{
				ActorImprintHandle handle = system->Load(m_bulletImprintRef);
				Actor* bullet = system->Instantiate(*GetOwner()->GetOwner(), handle);
				Bullet* bulletComponent = bullet ? bullet->GetComponentByClass<Bullet>() : nullptr;
				if (bulletComponent)
				{
					Vector3 spawnPosition = transform->GetWorldPosition() + forwardDirection * 1.0f; // プレイヤーの前方に1.0fの距離でスポーン
					spawnPosition.y += 0.7f; // プレイヤーの高さに合わせて少し上に調整
					bulletComponent->Launch(spawnPosition, forwardDirection, m_bulletSpeed);

					// 弾を発射したときのサウンド再生
					if (m_fireSoundRef.HasValue())
					{
						auto context = GetEngineContext();
						if (context && context->pAssetManager && context->pAudioManager)
						{
							const AudioHandle sound = context->pAssetManager->GetAudioHandle(m_fireSoundRef.GetGuid());
							if (sound != InvalidAudioHandle)
							{
								context->pAudioManager->Play(sound);
							}
						}
					}
				}
			}
		}
	}

	//-----------------------
	// プレイヤーの移動
	//-----------------------
	//前方向に向かって移動する
	if (transform)
	{
		const  float currentMoveSpeed = m_isAiming ? m_moveSlowSpeed : m_moveSpeed;

		if (inputInfo.key.w.down)
		{
			Vector3 horizontalForward = forwardDirection;
			horizontalForward.y = 0.0f; // 水平方向のみに制限
			horizontalForward = horizontalForward.Normalized();
			m_velocity = horizontalForward * currentMoveSpeed;
		}
		else if (inputInfo.key.s.down)
		{
			Vector3 horizontalBackward = transform->GetLocalForward() * -1.0f;
			horizontalBackward.y = 0.0f; // 水平方向のみに制限
			horizontalBackward = horizontalBackward.Normalized();
			m_velocity = horizontalBackward * currentMoveSpeed;
		}
		else if (inputInfo.key.a.down)
		{
			Vector3 horizontalLeft = transform->GetLocalLeft();
			horizontalLeft.y = 0.0f; // 水平方向のみに制限
			horizontalLeft = horizontalLeft.Normalized();
			m_velocity = horizontalLeft * currentMoveSpeed;
		}
		else if (inputInfo.key.d.down)
		{
			Vector3 horizontalRight = transform->GetLocalRight();
			horizontalRight.y = 0.0f; // 水平方向のみに制限
			horizontalRight = horizontalRight.Normalized();
			m_velocity = horizontalRight * currentMoveSpeed;
		}
		else
		{
			m_velocity = Vector3::Zero();
		}

		// 移動ベクトルを適用
		transform->SetLocalPosition(transform->GetLocalPosition() + m_velocity * deltaTime);
	}

}
void Player::LateUpdate()
{
	auto collisionInfo = m_collider ? m_collider->GetCollisionInfos() : std::vector<CollisionInfo>();

	// 壁or敵と衝突した際の押し戻し処理
	for (const auto& info : collisionInfo)
	{
		DBG("Player collided with: %s", info.opponent ? info.opponent->GetOwner()->GetName().c_str() : "Unknown");
		if (info.opponent)
		{
			auto opponentActor = info.opponent->GetOwner();
			if (opponentActor)
			{
				auto tagRegistry = TagRegistry::Get();
				auto opponentTag = opponentActor->GetTag();
				auto tagName = tagRegistry.GetName(opponentTag);

				if (tagName == "Wall")
				{
					DBG("Player collided with a wall: %s", opponentActor->GetName().c_str());
				}
				else if (tagName == "Enemy")
				{
					DBG("Player collided with an enemy: %s", opponentActor->GetName().c_str());
				}
				else
				{
					DBG("Player collided with an object of unknown tag: %s", opponentActor->GetName().c_str());
				}

				// 衝突した相手の方向に押し戻す
				auto transform = GetOwner()->GetComponentByClass<Transform>();
				if (transform)
				{
					if (info.state == COLLISION_STATE::COLLISION_ENTER || info.state == COLLISION_STATE::COLLISION_STAY)
					{
						// 衝突時の押し戻し処理
						Vector3 pushBack = -info.penetrationDepth;
						pushBack.y = 0.0f; // 水平方向のみに制限
						transform->SetLocalPosition(transform->GetLocalPosition() + pushBack); // 適当な距離で押し戻す

						// 速度ベクトルをリセット
						m_velocity = Vector3::Zero();
					}
				}
			}
			else
			{
				DBG("Player collided with an unknown object.");
			}
		}
	}
}


void Player::Destroy() {}
