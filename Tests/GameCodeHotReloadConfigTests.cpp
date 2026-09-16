#include "Tools/GameCodeHotReloadConfig.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAILED: " << message << '\n';
		std::exit(1);
	}

	bool SamePath(const std::string& actual, const std::filesystem::path& expected)
	{
		return std::filesystem::path(actual).lexically_normal() == expected.lexically_normal();
	}
}

int main()
{
	const std::filesystem::path root = "C:/Projects/101Engine";

	const auto debug = ResolveGameCodeHotReloadConfig(root.string(), "Debug");
	Check(debug.has_value(), "Debug configuration resolves");
	Check(debug->compilerFlags.find("/MDd") != std::string::npos,
		"Debug uses the debug DLL runtime");
	Check(debug->compilerFlags.find("/D_ITERATOR_DEBUG_LEVEL=2") != std::string::npos,
		"Debug uses the debug MSVC STL ABI");
	Check(SamePath(debug->frameworkLibrary, root / "build/lib/Debug/101Framework.lib"),
		"Debug links the Debug Framework import library");
	Check(SamePath(debug->stagedDll, root / "build/bin/Debug/GameCode.staged.dll"),
		"Debug stages the DLL in the Debug output directory");

	const auto release = ResolveGameCodeHotReloadConfig(root.string(), "Release");
	Check(release.has_value(), "Release configuration resolves");
	Check(release->compilerFlags.find("/MD ") != std::string::npos,
		"Release uses the release DLL runtime");
	Check(release->compilerFlags.find("/D_ITERATOR_DEBUG_LEVEL=0") != std::string::npos,
		"Release uses the release MSVC STL ABI");
	Check(SamePath(release->frameworkLibrary, root / "build/lib/Release/101Framework.lib"),
		"Release links the Release Framework import library");
	Check(SamePath(release->stagedDll, root / "build/bin/Release/GameCode.staged.dll"),
		"Release stages the DLL in the Release output directory");
	Check(debug->objectDirectory != release->objectDirectory,
		"Debug and Release object files cannot be mixed");

	Check(!ResolveGameCodeHotReloadConfig(root.string(), "RelWithDebInfo").has_value(),
		"Unsupported configurations are rejected");

	std::cout << "GameCode hot reload configuration tests passed.\n";
	return 0;
}
