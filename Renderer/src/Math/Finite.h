#pragma once

#include "Renderer/Math.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace rendering
{

inline bool Finite(const Matrix4x4& m)
{
	for (const auto& row : m.m)
	{
		for (float f : row)
		{
			if (!std::isfinite(f))
			{
				return false;
			}
		}
	}
	return true;
}

inline bool Finite(const std::array<float, 4>& a)
{
	return std::all_of(a.begin(), a.end(),[](float x) { return std::isfinite(x); });
}

inline bool Finite(const Vector4& v)
{
	return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.w);
}

inline bool Finite(const Vector3& v)
{
	return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline bool Finite(const Vector2& v)
{
	return std::isfinite(v.x) && std::isfinite(v.y);
}

}