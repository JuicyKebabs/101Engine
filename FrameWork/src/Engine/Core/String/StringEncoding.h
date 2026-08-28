#pragma once

#include <string>
#include <string_view>

namespace StringEncoding
{
    // Convert between UTF-16, used by Win32 wide-character APIs, and UTF-8.
    // An empty result represents either empty input or a conversion failure.
    std::string WideToUtf8(std::wstring_view value);
    std::wstring Utf8ToWide(std::string_view value);
}
