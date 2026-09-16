#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/Component/SpriteRenderer.h"

class Explosion : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
    static std::optional<TypeMetadata> BuildMetadata();

private:
	float m_duration = 1.0f; // 爆発の表示時間（秒）
	float m_elapsedTime = 0.0f; // 爆発の経過時間（秒）
	int m_spriteIndex = 0; // 現在のスプライトインデックス
	int m_totalSprites = 0; // スプライトの総数
	SpriteRenderer* m_spriteRenderer = nullptr; // 爆発のスプライトレンダラーへのポインタ
};

