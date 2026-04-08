#pragma once
#include <chrono>

class Time
{
public:
	Time() = default;	// Constructor
	~Time() = default;	// Destructor

	void Update();				// Update time
	float GetDeltaTime() const;	// Get delta time

private:

	std::chrono::steady_clock::time_point m_lastTime;	// Last time point
};