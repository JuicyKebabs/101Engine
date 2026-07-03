#include "Guid.h"
#include <windows.h>

std::string Guid::ToString() const
{
	// Convert the GUID to a wide string representation
	wchar_t buf[64] = {};
	StringFromGUID2(value, buf, 64);  // "{3F2A9C1E-8B47-4D6A-9E21-7C8F0A1B2D3E}"

	// Convert the wide string to a UTF-8 encoded std::string
	int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
	std::string result(len - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, buf, -1, result.data(), len, nullptr, nullptr);

	return result;
}

Guid Guid::FromString(const std::string& str)
{
	// Convert the string representation of the GUID to a GUID structure
	std::wstring wstr(str.begin(), str.end());
	Guid g;
	CLSIDFromString(wstr.c_str(), &g.value);
	return g;
}