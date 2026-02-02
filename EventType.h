#pragma once

enum EventType
{
	NONE,
	ITEM_PICKUP, // int teamID
	TAKE_DAMAGE, // int teamID, float damage
	UPDATE_HP_UI, // int teamID, float newHP
	UPDATE_BULLET_UI, // int teamID, int newBulletCount
	GAME_OVER, // int winningTeamID
	GAME_CLEAR,
	ADD_EFFECT_COMMAND,
	ADD_EFFECT_SP,
	RAYCAST_HIT, 
	FIRE_PLAYER_BULLET_NORMAL,
	FIRE_ENEMY_BULLET_NORMAL,
	CONTROL_RETICLE,
	SET_AIM_POINT,
	SET_ENEMY_TARGET,
	CHANGE_PLAYER_HP,
	CHANGE_BOSS_HP,
	CALL_CAMERA_SHAKE,
	ACTIVATE_BOSS_HP_BAR,
	DEACTIVATE_BOSS_HP_BAR,
	ADD_ENEMY_SPAWN_COMMAND,
	ADD_FIELD_SPAWN_COMMAND,
	SEND_PROGRESS_TO_ENEMY_MANAGER,
	SEND_PROGRESS_TO_STAGE_DIRECTOR,
	ACTIVATE_BOSS_ENEMY,
	ACTIVATE_GAME_OVER_UI,
	ACTIVATE_GAME_CLEAR_UI,
	CHANGE_SCENE, // SCENE_TYPE
	START_FADE_IN, // float duration
	START_FADE_OUT, // float duration
	ADD_SCORE, // int score
};

//イベントデータ構造体
struct EventData
{
	EventType type = EventType::NONE;
	uint64_t id = 0;
};

//イベントデータリストから特定のイベントデータを検索するヘルパー関数
inline static EventData FindEventData(
	const std::vector<EventData>& eventDataList,
	EventType type
)
{
	for (const auto& eventData : eventDataList)
	{
		if (eventData.type == type)
		{
			return eventData;
		}
	}

	return EventData{ EventType::NONE, 0 };
}