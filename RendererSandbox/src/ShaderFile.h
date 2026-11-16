#pragma once

#include <Windows.h>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace sandbox
{
	inline std::filesystem::path GetExecutableDirectory()
	{
		std::vector<wchar_t> buffer(512);
		for (;;)
		{
			const DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (copied == 0)
			{
				throw std::runtime_error("GetModuleFileNameW failed");
			}
			if (copied < buffer.size())
			{
				return std::filesystem::path(buffer.data()).parent_path();
			}
			buffer.resize(buffer.size() * 2);
		}
	}

}
