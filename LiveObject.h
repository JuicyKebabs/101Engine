#pragma once
#include "Actor.h"

class LiveObject
{
public:
	LiveObject(
		int maxHitNum
	) : m_maxHitNum(maxHitNum) {};
	~LiveObject() {};

	void TakeHit(int damage);
	void Update();
	void ResetHitStatus();
	bool IsDestroyed() const;

protected:
	int m_maxHitNum = -1;	// Maximum hit points(-1 means infinite)
	int m_hitNum = 0;
	bool m_isHit = false;
	int m_hitFrames = 0;

	virtual void TakeHitOverride(int damage) = 0;
};
