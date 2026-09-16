#include "GameCodeHotReloadConfig.h"

#include <filesystem>

std::optional<GameCodeHotReloadConfig> ResolveGameCodeHotReloadConfig(
	const std::string& projectRoot,
	const std::string& configuration)
{
	if (configuration != "Debug" && configuration != "Release")
	{
		return std::nullopt;
	}

	const std::filesystem::path root(projectRoot);
	const std::filesystem::path build = root / "build";
	const std::filesystem::path output = build / "bin" / configuration;
	const std::filesystem::path libraries = build / "lib" / configuration;

	GameCodeHotReloadConfig result;
	result.configuration = configuration;
	result.compilerFlags = configuration == "Debug"
		? "/MDd /Od /D_DEBUG /D_ITERATOR_DEBUG_LEVEL=2"
		: "/MD /O2 /Ob2 /DNDEBUG /D_ITERATOR_DEBUG_LEVEL=0";
	result.objectDirectory = (build / "GameCode_hotreload" / configuration / "obj").string();
	result.frameworkLibrary = (libraries / "101Framework.lib").string();
	result.stagedDll = (output / "GameCode.staged.dll").string();
	result.stagedLibrary = (libraries / "GameCode.staged.lib").string();
	return result;
}
