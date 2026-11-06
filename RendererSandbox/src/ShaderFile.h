#pragma once

#include <Windows.h>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sandbox
{
	// 任意のstd::exception::what()とは異なり、これらのメッセージはUTF-8とする。
	class ShaderFileError : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	inline ShaderFileError MakeShaderFileError(const char* reason, const std::filesystem::path& path)
	{
		const auto utf8Path = path.u8string();
		return ShaderFileError(std::string(reason) + ": " + std::string(utf8Path.begin(), utf8Path.end()));
	}

	inline std::filesystem::path GetExecutableDirectory()
	{
		std::vector<wchar_t> buffer(512);
		for (;;)
		{
			const DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (copied == 0)
			{
				throw ShaderFileError("GetModuleFileNameW failed");
			}
			if (copied < buffer.size())
			{
				return std::filesystem::path(buffer.data()).parent_path();
			}
			buffer.resize(buffer.size() * 2);
		}
	}

	inline std::vector<std::byte> ReadShader(const std::filesystem::path& name)
	{
		const auto path = GetExecutableDirectory() / L"shaders" / name;
		std::ifstream file(path, std::ios::binary | std::ios::ate);

		if (!file)
		{
			throw MakeShaderFileError("Cannot open compiled shader file", path);
		}

		const auto size = file.tellg();

		if (size <= 0)
		{
			throw MakeShaderFileError("Cannot read compiled shader file size or file is empty", path);
		}

		std::vector<std::byte> result(static_cast<std::size_t>(size));
		file.seekg(0);

		if (!file.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size())))
		{
			throw MakeShaderFileError("Shader file read failed", path);
		}

		return result;
	}
}
