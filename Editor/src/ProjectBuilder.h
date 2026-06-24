#pragma once
#include <string>

class ProjectBuilder
{
public:
    // Re-run CMake configure (picks up newly added GameCode files).
    // Returns true on success, false otherwise.
    static bool Reconfigure();

    // Build the given target/config via "cmake --build".
    static bool Build(
        const std::string& target = "101Game",
        const std::string& config = "Debug", 
        bool buildDependencies = true
    );

    // Reconfigure + Build in sequence.
    static bool ReconfigureAndBuild(
        const std::string& target = "101Game", 
        const std::string& config = "Debug",
		bool buildDependencies = true
        );

	// Build the GameCode for hot reload (Debug config).
    // This method does't use MSBuild system.
	// Call cl.exe and link.exe directly to build the GameCode for hot reload.
	static bool BuildGameCodeForHotReload(const std::string& config = "Debug");
};
