#include "EventManager.h"

/*
//シングルトンインスタンス取得
EventManager *EventManager::GetInstance()
{
	static EventManager instance; //シングルトンインスタンス
	return &instance;
}

void EventManager::TakeDamage(int teamID, float damage) //ダメージ処理関数
{
	teamHP[teamID] -= damage;
	if (teamHP[teamID] < 0.0f)
	{
		teamHP[teamID] = 0.0f;
	}
}

void EventManager::AddBullets(int count)
{
	for (int& teamBullet : teamBulletCount)
	{
		teamBullet += count;
		if (teamBullet > bulletCountMax)
		{
			teamBullet = bulletCountMax;
		}
	}
}

bool EventManager::UseBullet(int teamID)
{
	if (teamBulletCount[teamID] > 0)
	{
		teamBulletCount[teamID]--;
		return true;
	}

	return false;
}
*/