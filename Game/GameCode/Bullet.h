#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/Component/Collider.h"

class Bullet : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
    static std::optional<TypeMetadata> BuildMetadata();

    // 発射
	void Launch(const Vector3& position, const Vector3& direction, float speed);

private:
	Vector3 m_direction = Vector3::Zero(); 
	float m_speed = 10.0f;
	float m_lifeTime = 5.0f;

	float m_damage = 1.0f; // ダメージ量

	Collider* m_collider = nullptr; // コライダーコンポーネントへのポインタ
};

