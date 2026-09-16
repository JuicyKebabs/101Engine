#pragma once
#include "Engine/Component/Behavior.h"
#include "Player.h"
#include "Engine/Resource/AssetReference.h"

enum class GameState
{
    Tutorial,
	Playing,
    Result
};

enum class GameResult
{
	Win,
	Lose
};

class GameManager : public Behavior
{
public:
    void Start() override;
    void PreUpdate() override;
    void Update() override;
    void LateUpdate() override;
    void Destroy() override;
    static std::optional<TypeMetadata> BuildMetadata();

	void DestroyTarget() { m_restTarget--; }	// ターゲットを破壊したときに呼ばれる関数

	float GetRestTime() const { return m_restTime; }	// 残り時間を取得

private:

	float m_restTime = 0.0f;	// ゲームの残り時間
	int m_restTarget = 0;		// 残りターゲット数

	GameState m_gameState = GameState::Tutorial;	// 現在のゲームの状態

	Player* m_player = nullptr;	// プレイヤーコンポーネントへのポインタ

    AssetReference<SceneAsset> m_nextScene;
	ActorReference m_tutorialUIActor;
	ActorReference m_resultUIActor;
	ActorReference m_timerUIActor;
	ActorReference m_clearImageActor;
	ActorReference m_failedImageActor;
	ActorReference m_titlePromptImageActor;
};
