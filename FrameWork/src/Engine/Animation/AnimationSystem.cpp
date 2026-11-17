#include "AnimationSystem.h"

#include "Engine/Actor/ActorHierarchyActivity.h"
#include "Engine/Component/AnimationPlayer.h"

#include <algorithm>

void AnimationSystem::Register(AnimationPlayer* player)
{
	if (std::find(m_players.begin(), m_players.end(), player) == m_players.end())
	{
		m_players.push_back(player);
	}
}

void AnimationSystem::Unregister(AnimationPlayer* player)
{
	std::erase(m_players, player);
}

void AnimationSystem::ProcessAnimation(float deltaTime)
{
	for (AnimationPlayer* player : m_players)
	{
		if (!player->IsDestroyed() && IsActorHierarchyActive(player->GetOwner()))
		{
			player->ProcessAnimation(deltaTime);
		}
	}
}