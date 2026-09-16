#pragma once
#include <chrono>
#include <memory>
#include <cstdint>
#include "Engine/Core/Debug/Debug.h"

//--------------------------------------------------------------------
// TimeManager class
// Manages time-related operations, including delta time calculation.
// -------------------------------------------------------------------

class TimeManager
{
public:
	static TimeManager& GetInstance();

	TimeManager(const TimeManager&) = delete;
	TimeManager& operator=(const TimeManager&) = delete;

	void Update();

	float GetDeltaTime() const { return m_deltaTime; }

private:
	TimeManager() = default;	// Constructor

	static inline std::unique_ptr<TimeManager> m_instance;	// Singleton instance
	
	std::chrono::high_resolution_clock::time_point m_currentTime = std::chrono::high_resolution_clock::now();	// Current time point

	float m_deltaTime = 0.0f;	// Delta time variable
};

namespace Time
{
	static float GetTimeSeconds();
}
