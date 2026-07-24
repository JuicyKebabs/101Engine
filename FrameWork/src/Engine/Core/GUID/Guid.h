#pragma once
#include <objbase.h>
#include <string>
#include <functional>
#include <cstring>

//-----------------------------------------------------------------------------------------
// Guid struct
// A generic, globally unique identifier (GUID) structure that wraps the Windows GUID type.
//-----------------------------------------------------------------------------------------

struct Guid
{
	GUID value{};// The underlying Windows GUID value

	// Operators for comparison
	bool operator==(const Guid& other) const
	{
		return std::memcmp(&value, &other.value, sizeof(GUID)) == 0;
	}
	bool operator!=(const Guid& other) const
	{
		return !(*this == other);
	}

	bool IsValid() const
	{
		static const GUID zero{};

		// Check if the GUID is not equal to the zero GUID
		return std::memcmp(&value, &zero, sizeof(GUID)) != 0;
	}

	// String conversion methods
	std::string ToString() const;
	static Guid FromString(const std::string& str);
	static bool TryParse(const std::string& str, Guid& outGuid);
};

// Hash function for using Guid as a key in unordered containers
namespace std
{
	template<>
	struct hash<Guid>
	{
		size_t operator()(const Guid& g) const
		{
			const uint64_t* p = reinterpret_cast<const uint64_t*>(&g.value);
			return std::hash<uint64_t>()(p[0]) ^ (std::hash<uint64_t>()(p[1]) << 1);
		}
	};
}