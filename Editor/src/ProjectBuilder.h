#pragma once
#include <string>

class ProjectBuilder
{
public:
    // Re-run CMake configure (picks up newly added GameCode files).
    // Returns true on success, false otherwise.
    static bool Reconfigure();

    // Build the given target/config via "cmake --build".
    static bool Build(const std::string& target = "101Game", const std::string& config = "Debug");

    // Reconfigure + Build in sequence.
    static bool ReconfigureAndBuild(const std::string& target = "101Game", const std::string& config = "Debug");
};
