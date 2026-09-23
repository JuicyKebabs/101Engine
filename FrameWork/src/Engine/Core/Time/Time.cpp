#include "Time.h"

TimeManager& TimeManager::GetInstance()
{
	if (!m_instance)
	{
		m_instance = std::unique_ptr<TimeManager>(new TimeManager());
	}

	return *m_instance;
}

void TimeManager::Update()
{
	auto previous = m_currentTime;
	m_currentTime = std::chrono::high_resolution_clock::now();
	m_deltaTime = std::chrono::duration<float>(m_currentTime - previous).count();

	if (m_deltaTime < 0.0f)
	{
		DBG("TimeManager: Delta time is negative, which is unexpected.");
		m_deltaTime = 0.0f;
	}
}
float TimeManager::GetPassedTime() const
{
	return std::chrono::duration<float>(m_currentTime - m_initiatedTime).count();
}

float Time::GetTimeSeconds()
{
	return TimeManager::GetInstance().GetDeltaTime();
}

float Time::GetPassedTime()
{
	return TimeManager::GetInstance().GetPassedTime();
}