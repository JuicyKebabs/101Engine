#pragma once
#include "Engine/Core/Math/Math.h"
#include "nlohmann/json.hpp"

namespace JsonMath
{
	inline nlohmann::json ToJson(const Vector2& value)		{ return { value.x, value.y }; }
	inline nlohmann::json ToJson(const Vector3& value)		{ return { value.x, value.y, value.z }; }
	inline nlohmann::json ToJson(const Vector4& value)		{ return { value.x, value.y, value.z, value.w }; }
	inline nlohmann::json ToJson(const Quaternion& value)	{ return { value.x, value.y, value.z, value.w }; }

	inline bool TryRead(const nlohmann::json& json, Vector2& outValue)
	{
		if (!json.is_array() || json.size() != 2) return false;
		if (!json[0].is_number() || !json[1].is_number()) return false;
		
		try
		{
			Vector2 parsed{json[0].get<float>(), json[1].get<float>()};
			outValue = parsed;
			return true;
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}

	inline bool TryRead(const nlohmann::json& json, Vector3& outValue)
	{
		if (!json.is_array() || json.size() != 3) return false;
		if (!json[0].is_number() || !json[1].is_number() || !json[2].is_number()) return false;

		try
		{
			Vector3 parsed{json[0].get<float>(), json[1].get<float>(), json[2].get<float>()};
			outValue = parsed;
			return true;
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}

	inline bool TryRead(const nlohmann::json& json, Vector4& outValue)
	{
		if (!json.is_array() || json.size() != 4) return false;
		if (!json[0].is_number() || !json[1].is_number() || !json[2].is_number() || !json[3].is_number()) return false;
		
		try
		{
			Vector4 parsed{ json[0].get<float>(), json[1].get<float>(), json[2].get<float>(), json[3].get<float>() };
			outValue = parsed;
			return true;
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}

	inline bool TryRead(const nlohmann::json& json, Quaternion& outValue)
	{
		if (!json.is_array() || json.size() != 4) return false;
		if (!json[0].is_number() || !json[1].is_number() || !json[2].is_number() || !json[3].is_number()) return false;
		
		try
		{
			Quaternion parsed{ json[0].get<float>(), json[1].get<float>(), json[2].get<float>(), json[3].get<float>() };
			outValue = parsed;
			return true;
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}
}