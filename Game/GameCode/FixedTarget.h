#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/ActorImprint/ActorImprint.h"
#include "GameManager.h"

class FixedTarget : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
    static std::optional<TypeMetadata> BuildMetadata();

    void Break();

private:
	GameManager* m_gameManager = nullptr;
	AssetReference<ActorImprint> m_explosionImprintRef; // 爆発のアクターインプリントへの参照
};

