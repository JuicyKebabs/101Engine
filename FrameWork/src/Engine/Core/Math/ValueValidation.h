#pragma once
#include "Engine/Core/Math/Math.h"
#include <cmath>

namespace ValueValidation
{
	inline bool Finite2(const Vector2& v) { return std::isfinite(v.x) && std::isfinite(v.y); }
	inline bool Finite3(const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
	inline bool UnitCoordinate2(const Vector2& v)
	{
		return Finite2(v) && v.x >= 0 && v.x <= 1 && v.y >= 0 && v.y <= 1;
	}
	inline bool NormalizeRotation(Quaternion& rotation)
	{
		const float lengthSquared = rotation.LengthSq();
		if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) return false;
		rotation = rotation.Normalized();
		return true;
	}
}
