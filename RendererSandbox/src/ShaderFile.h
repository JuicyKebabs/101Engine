#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace sandbox
{
	inline std::vector<std::byte> ReadShader(const char* name)
	{
		std::ifstream file(std::filesystem::path(R4_SHADER_DIRECTORY) / name, std::ios::binary | std::ios::ate);

		if (!file)
		{
			throw std::runtime_error("compiled Shader file is missing");
		}

		const auto size = file.tellg();

		if (size <= 0)
		{
			throw std::runtime_error("compiled Shader file is empty");
		}

		std::vector<std::byte> result(static_cast<std::size_t>(size));
		file.seekg(0);

		if (!file.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size())))
		{
			throw std::runtime_error("Shader file read failed");
		}

		return result;
	}
}