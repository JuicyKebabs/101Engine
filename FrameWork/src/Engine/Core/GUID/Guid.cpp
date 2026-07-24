#include "Guid.h"
#include <windows.h>

std::string Guid::ToString() const
{
	wchar_t buffer[64] = {};

	const int wideLength = StringFromGUID2(value, buffer, 64);
	if (wideLength <= 1) return {};

	// Calculate the length of the content without the null terminator
	const int contentLength = wideLength - 1;

	const int utf8Length = WideCharToMultiByte(
		CP_UTF8,
		0,
		buffer,
		contentLength,
		nullptr,
		0,
		nullptr,
		nullptr
	);

	if (utf8Length <= 0) return {};

	// Create a string with the required size to hold the UTF-8 representation
	std::string result(static_cast<size_t>(utf8Length), '\0');

	// Convert the wide string to UTF-8 and store it in the result string
	if (WideCharToMultiByte(
		CP_UTF8,
		0,
		buffer,
		contentLength,
		result.data(),
		utf8Length,
		nullptr,
		nullptr) == 0
		)
	{
		return {};
	}

	return result;
}

Guid Guid::FromString(const std::string& str)
{
	Guid guid{};
	TryParse(str, guid);
	return guid;
}

bool Guid::TryParse(const std::string& str, Guid& outGuid)
{
	if (str.empty()) return false;

	// Convert the UTF-8 string to a wide string
	const int wideLength = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		str.data(),
		static_cast<int>(str.size()),
		nullptr,
		0
	);

	if (wideLength <= 0) return false;

	// Create a wide string with the required size to hold the wide representation
	std::wstring wideString(
		static_cast<size_t>(wideLength),
		L'\0'
	);

	// Convert the UTF - 8 string to a wide string and store it in the wideString
	if (MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		str.data(),
		static_cast<int>(str.size()),
		wideString.data(),
		wideLength) == 0
		)
	{
		return false;
	}

	Guid parsed{};

	// Parse the wide string representation of the GUID to a GUID structure
	const HRESULT result = CLSIDFromString(
		wideString.c_str(),
		&parsed.value
	);

	if (FAILED(result) || !parsed.IsValid())
	{
		return false;
	}

	outGuid = parsed;
	return true;
}