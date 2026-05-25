#pragma once
#include <chrono>
#include <memory>
#include <cstdint>
#include "Engine/Core/Debug/Debug.h"

class TimeManager
{
public:
	static TimeManager& GetInstance(){
		if(!m_instance){
			m_instance = std::unique_ptr<TimeManager>(new TimeManager());
		}
		return *m_instance;
	}

	TimeManager(const TimeManager&) = delete;
	TimeManager& operator=(const TimeManager&) = delete;

	void Update() {
		auto previous = m_currentTime;
		m_currentTime = std::chrono::high_resolution_clock::now();
		m_deltaTime = std::chrono::duration<float>(m_currentTime - previous).count();
	}

	float GetDeltaTime() const { return m_deltaTime; }

private:
	TimeManager() = default;	// Constructor

	static inline std::unique_ptr<TimeManager> m_instance;	// Singleton instance
	
	std::chrono::high_resolution_clock::time_point m_currentTime;	// Current time point
	float m_deltaTime = 0.0f;										// Delta time variable
};

namespace Time
{
	static float GetTimeSeconds()
	{
		return TimeManager::GetInstance().GetDeltaTime();
	}
}