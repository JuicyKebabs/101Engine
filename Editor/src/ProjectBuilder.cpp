#include "ProjectBuilder.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <fstream>
#include <cstdlib>

// NOTE: cmake is invoked via a generated .bat file rather than passing the
// command directly to system(). Passing a command containing multiple
// double-quoted paths (e.g. `"...cmake.exe" -S "..." -B "..." > "...log" 2>&1`)
// directly to system() causes cmd.exe to mis-parse the quoting (it wraps the
// whole string in an extra layer of quotes), which made cmake fail with
// exit code 1 even though the exact same command worked when typed manually
// into a command prompt. Writing the command to a .bat file and calling
// system() with just the file path avoids the double-parsing problem,
// because system() only sees a simple unquoted path, and cmd.exe parses the
// .bat file's contents only once.
static const std::string kCMakePath =
    "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe";

bool ProjectBuilder::Reconfigure()
{
    std::string projectRoot = PathManager::GetProjectRoot();
    std::string buildDir = projectRoot + "/build";

    std::string batPath = projectRoot + "/reconfigure.bat";
    std::ofstream bat(batPath);
    bat << "\"" << kCMakePath << "\" -S \"" << projectRoot << "\" -B \"" << buildDir
        << "\" -G \"Visual Studio 17 2022\" -A x64 > \"" << projectRoot << "/cmake_reconfigure.log\" 2>&1\n";
    bat.close();

    DBG("ProjectBuilder: Running reconfigure via batch file...");
    int result = system(batPath.c_str());

    if (result != 0)
    {
        DBG("ProjectBuilder: Reconfigure failed (code %d)", result);
        return false;
    }

    DBG("ProjectBuilder: Reconfigure succeeded.");
    return true;
}

bool ProjectBuilder::Build(const std::string& target, const std::string& config)
{
    std::string projectRoot = PathManager::GetProjectRoot();
    std::string buildDir = projectRoot + "/build";

    std::string batPath = projectRoot + "/build_project.bat";
    std::ofstream bat(batPath);
    bat << "\"" << kCMakePath << "\" --build \"" << buildDir
        << "\" --target " << target << " --config " << config
        << " > \"" << projectRoot << "/cmake_build.log\" 2>&1\n";
    bat.close();

    DBG("ProjectBuilder: Building target '%s' (%s) via batch file...", target.c_str(), config.c_str());
    int result = system(batPath.c_str());

    if (result != 0)
    {
        DBG("ProjectBuilder: Build failed (code %d)", result);
        return false;
    }

    DBG("ProjectBuilder: Build succeeded.");
    return true;
}

bool ProjectBuilder::ReconfigureAndBuild(const std::string& target, const std::string& config)
{
    if (!Reconfigure()) return false;
    return Build(target, config);
}
