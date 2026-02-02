#include "LiveObject.h"

//take hit function
void LiveObject::TakeHit(int damage)
{
	if (m_maxHitNum != -1)
	{//if not infinite hit points
		m_hitNum += damage;
	}

	m_isHit = true;
	m_hitFrames = 0;

	TakeHitOverride(damage);
}

//update function
void LiveObject::Update()
{
	if (m_isHit)
	{
		m_hitFrames++;

		if (m_hitFrames > 5)
		{
			m_hitFrames = 0;
			m_isHit = false;
		}
	}
}

//reset hit status function
void LiveObject::ResetHitStatus()
{
	m_isHit = false;
	m_hitNum = 0;
	m_hitFrames = 0;
}

//is destroyed function
bool LiveObject::IsDestroyed() const
{
	if (m_maxHitNum == -1)
	{
		return false;
	}

	return m_hitNum >= m_maxHitNum;
}