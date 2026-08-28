#include "StringEncoding.h"

#include <limits>
#include <windows.h>

namespace StringEncoding
{
    std::string WideToUtf8(std::wstring_view value)
    {
        if (value.empty() || value.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
        {
            return {};
        }

        const int sourceLength = static_cast<int>(value.size());
        const int resultLength = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            sourceLength,
            nullptr,
            0,
            nullptr,
            nullptr
        );
        if (resultLength <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(resultLength), '\0');
        if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            sourceLength,
            result.data(),
            resultLength,
            nullptr,
            nullptr
        ) != resultLength)
        {
            return {};
        }

        return result;
    }

    std::wstring Utf8ToWide(std::string_view value)
    {
        if (value.empty() || value.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
        {
            return {};
        }

        const int sourceLength = static_cast<int>(value.size());
        const int resultLength = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            sourceLength,
            nullptr,
            0
        );
        if (resultLength <= 0)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(resultLength), L'\0');
        if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            sourceLength,
            result.data(),
            resultLength
        ) != resultLength)
        {
            return {};
        }

        return result;
    }
}
