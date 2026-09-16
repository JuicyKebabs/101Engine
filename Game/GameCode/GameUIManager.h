#pragma once
#include "Engine/Component/Behavior.h"
#include "Engine/Actor/ActorReference.h"

class GameUIManager : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
    static std::optional<TypeMetadata> BuildMetadata();

private:
	ActorReference m_gameManagerActor;  // GameManagerアクターへの参照
	ActorReference m_digit100sActor;    // 100の位のUIImageアクターへの参照
	ActorReference m_digit10sActor;     // 10の位のUIImageアクターへの参照
	ActorReference m_digit1sActor;      // 1の位のUIImageアクターへの参照
	ActorReference m_digit01sActor;     // 0.1の位のUIImageアクターへの参照
	ActorReference m_digit001sActor;    // 0.01の位のUIImageアクターへの参照
};

