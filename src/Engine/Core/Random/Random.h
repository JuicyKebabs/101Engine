#pragma once
#include <random>

class Random
{
public:
	explicit Random(uint32_t seed) : mt(seed) {}

	float GetFloat(float min, float max)
	{
		std::uniform_real_distribution<float> dist(min, max);
		return dist(mt);
	}

	int GetInt(int min, int max)
	{
		std::uniform_int_distribution<int> dist(min, max);
		return dist(mt);
	}

private:
	std::mt19937 mt;
};