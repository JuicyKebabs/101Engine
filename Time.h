#pragma once
#include <chrono>

class Time
{
public:
	static Time* GetInstance()
	{
		static Time instance;
		return &instance;
	}
	void Update();				// Update time
	float GetDeltaTime() const;	// Get delta time

private:
	Time() = default;	// Constructor
	~Time() = default;	// Destructor

	std::chrono::steady_clock::time_point m_lastTime;	// Last time point
};