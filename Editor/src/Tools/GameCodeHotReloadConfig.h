#pragma once

#include <optional>
#include <string>

struct GameCodeHotReloadConfig
{
	std::string configuration;
	std::string compilerFlags;
	std::string objectDirectory;
	std::string frameworkLibrary;
	std::string stagedDll;
	std::string stagedLibrary;
};

// Resolves all configuration-sensitive hot-reload inputs from one explicit
// build configuration. Unsupported configurations are rejected so a GameCode
// DLL with an unknown ABI cannot be loaded into the Editor process.
std::optional<GameCodeHotReloadConfig> ResolveGameCodeHotReloadConfig(
	const std::string& projectRoot,
	const std::string& configuration);
