#pragma once

#include <vector>

class AnimationPlayer;

class AnimationSystem
{
public:
	void Register(AnimationPlayer* player);
	void Unregister(AnimationPlayer* player);
	void ProcessAnimation(float deltaTime);

private:
	std::vector<AnimationPlayer*> m_players;
};