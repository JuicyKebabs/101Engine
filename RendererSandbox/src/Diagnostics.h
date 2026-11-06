#pragma once

#include <Windows.h>
#include <limits>
#include <string>
#include <string_view>

namespace sandbox
{
	inline std::wstring Utf8ToWide(std::string_view text)
	{
		if (text.empty())
		{
			return {};
		}
		constexpr auto fallback = L"Invalid UTF-8 diagnostic.";
		if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
		{
			return fallback;
		}
		const int size = static_cast<int>(text.size());
		const int required = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
		if (required == 0)
		{
			return fallback;
		}
		std::wstring converted(static_cast<std::size_t>(required), L'\0');
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			text.data(), size, converted.data(), required) != required)
		{
			return fallback;
		}
		return converted;
	}

	// 診断用のメモリ確保や変換が失敗しても、致命的なGPUエラー時の終了処理を迂回させない。
	inline void WriteDiagnostic(std::string_view text) noexcept
	{
		try
		{
			const auto message = Utf8ToWide(text);
			OutputDebugStringW(message.c_str());
		}
		catch (...)
		{
			OutputDebugStringW(L"[RendererSandbox] Diagnostic output failed.\n");
		}
	}
}
