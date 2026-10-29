#pragma once

#include <Windows.h>
#include <filesystem>
#include <stdexcept>
#include <vector>

// The shader directory is relative to the executable, never the process working
// directory. Grow the UTF-16 buffer so installations with long/non-ASCII paths
// have the same behavior as a development build.
inline std::filesystem::path GetExecutableDirectory()
{
    std::vector<wchar_t> buffer(512);

    for (;;)
    {
        const DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

        if (copied == 0)
        {
            throw std::runtime_error("GetModuleFileNameW failed.");
        }

        if (copied < buffer.size())
        {
            return std::filesystem::path(buffer.data()).parent_path();
        }

        buffer.resize(buffer.size() * 2);
    }
}
