#pragma once
#include <windows.h>
#include <string>
#include <cstdarg>

// Out put debug string
static void DBG(const char* fmt, ...)
{
	char buf[4096];

	va_list args;
	va_start(args, fmt);
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
	va_end(args);

	size_t len = strlen(buf);
	if (len > 0 && buf[len - 1] != '\n' && len < sizeof(buf) - 1)
	{
		buf[len] = '\n';
		buf[len + 1] = '\0';

	}
	OutputDebugStringA(buf);
}
