#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/Component/AnimationPlayer.h"

class AnimationTest : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
    static std::optional<TypeMetadata> BuildMetadata();

	AnimationPlayer* m_animationPlayer;
};

