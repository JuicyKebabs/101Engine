#include "ProjectBuilder.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <fstream>
#include <cstdlib>
#include <filesystem>

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

// Helper function to get the Visual Studio installation path using vswhere.exe.
// This function is used to resolve the path to cmake.exe and vcvars64.bat.
static std::string GetVSInstallPath()
{
    FILE* pipe = _popen(
        "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\""
        " -latest -property installationPath 2>NUL", "r");

    if (!pipe) return "";

    char buf[512] = {};
    std::string path;

    if (fgets(buf, sizeof(buf), pipe))
    {
        path = buf;
        while (!path.empty() &&
            (path.back() == '\n' || path.back() == '\r' || path.back() == ' '))
            path.pop_back();
    }

    _pclose(pipe);

    return path;
}

// Helper function to resolve the path to cmake.exe using the "where" command.
static std::string ResolveCMakePath()
{
    std::string installPath = GetVSInstallPath();
    if (!installPath.empty())
    {
        std::string cmakePath = installPath +
            "\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe";
        DBG("ProjectBuilder: cmake found at '%s'", cmakePath.c_str());
        return cmakePath;
    }
    DBG("ProjectBuilder: vswhere.exe not found, falling back to 'cmake' in PATH");
    return "cmake";
}

// Resolve the path to cmake.exe at static initialization time.
static const std::string kCMakePath = ResolveCMakePath();

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

bool ProjectBuilder::Build(const std::string& target, const std::string& config, bool buildDependencies)
{
	// Construct paths
    std::string projectRoot = PathManager::GetProjectRoot();
    std::string buildDir = projectRoot + "/build";

	// Generate batch file to invoke cmake --build with proper quoting and output redirection
    std::string batPath = projectRoot + "/build_project.bat";
    std::ofstream bat(batPath);

    bat << "\"" << kCMakePath << "\" --build \"" << buildDir
        << "\" --target " << target << " --config " << config;

	// Skip building dependencies if requested 
    // (e.g. to avoid disconnecting static singletons from target)
	if (!buildDependencies)
    {
        bat << " -- /p:BuildProjectReferences=false";
    }

    bat << " > \"" << projectRoot << "/cmake_build.log\" 2>&1\n";
    bat.close();

	// Run the batch file and check the result
    DBG("ProjectBuilder: Building target '%s' (%s)%s via batch file...",
        target.c_str(), config.c_str(),
        buildDependencies ? "" : " (skip deps)");
    int result = system(batPath.c_str());

    if (result != 0)
    {
        DBG("ProjectBuilder: Build failed (code %d)", result);
        return false;
    }

    DBG("ProjectBuilder: Build succeeded.");
    return true;
}

bool ProjectBuilder::ReconfigureAndBuild(const std::string& target, const std::string& config, bool buildDependencies)
{
    if (!Reconfigure()) return false;
    return Build(target, config, buildDependencies);
}

bool ProjectBuilder::BuildGameCodeForHotReload(const std::string& config)
{
    namespace fs = std::filesystem;

	// Construct paths for the project root and Visual Studio installation
    std::string projectRoot = PathManager::GetProjectRoot();
    std::string vsPath = GetVSInstallPath();
    if (vsPath.empty())
    {
        DBG("ProjectBuilder: VS installation not found via vswhere.exe");
        return false;
    }

	// Construct paths for vcvars64.bat, game code directory, and framework include directory
    std::string vcvars = vsPath + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
    std::string gameCodeDir = projectRoot + "\\Game\\GameCode";
    std::string frameworkInc = projectRoot + "\\Framework\\src";

	// Construct paths for location of outputting build artifacts
    std::string objDir = projectRoot + "\\build\\GameCode_hotreload\\obj";
    std::string frameworkLib = projectRoot + "\\build\\lib\\" + config + "\\101Framework.lib";
    std::string outputDll = projectRoot + "\\build\\bin\\" + config + "\\GameCode.dll";
    std::string outputLib = projectRoot + "\\build\\lib\\" + config + "\\GameCode.lib";
    std::string logPath = projectRoot + "\\hotreload_compile.log";
    std::string batPath = projectRoot + "\\build_hotreload.bat";

    // Collect all source files for hot reload
    std::vector<std::string> sources;
    for (const auto& entry : fs::recursive_directory_iterator(gameCodeDir))
    {
		if (entry.path().extension() == ".cpp") sources.push_back(entry.path().string());
    }

    // Check if hot reload is needed
    if (sources.empty())
    {
        DBG("ProjectBuilder: No .cpp files found in Game/GameCode/");
        return false;
    }

	// Create the directory for saving .obj files if it doesn't exist
	fs::create_directories(objDir);

	// Switch runtime library flag based on build configuration
	// Debug : /MDd, Release : /MD
    std::string rtFlag = (config == "Debug") ? "/MDd" : "/MD";

	// Generate batch file to compile and link GameCode for hot reload
    {
        std::ofstream bat(batPath);
        bat << "@echo off\n\n";

		// Call vcvars64.bat to set up the Visual Studio environment
		// In order to enable cl.exe, link.exe and other included tools to be used in the batch file
        bat << "call \"" << vcvars << "\" >nul 2>&1\n";
        bat << "if errorlevel 1 (\n";
        bat << "    echo [HotReload] vcvars64.bat failed\n";
        bat << "    exit /b 1\n";
        bat << ")\n\n";

		// Compile all .cpp files into .obj files
        // Genarate obj files by giving all .cpp files to cl.exe.
        bat << "echo [HotReload] Compiling " << (int)sources.size() << " files...\n";
        bat << "cl.exe /nologo " << rtFlag << " /Z7 /EHsc /std:c++latest /DNOMINMAX /c";
        bat << " /I\"" << gameCodeDir << "\"";
        bat << " /I\"" << frameworkInc << "\"";
        bat << " /Fo\"" << objDir << "\\\\\"";
        for (const auto& src : sources)
            bat << " \"" << src << "\"";
        bat << " > \"" << logPath << "\" 2>&1\n";
        bat << "if errorlevel 1 (\n";
        bat << "    echo [HotReload] Compile FAILED - see hotreload_compile.log\n";
        bat << "    exit /b 1\n";
        bat << ")\n\n";

		// Link all .obj files into GameCode.dll and GameCode.lib
        bat << "echo [HotReload] Linking...\n";
        bat << "link.exe /nologo /DLL /OPT:NOREF /DEBUG:NONE";
        bat << " /OUT:\"" << outputDll << "\"";
        bat << " /IMPLIB:\"" << outputLib << "\"";
        bat << " \"" << frameworkLib << "\"";
        for (const auto& src : sources)
        {
            std::string stem = fs::path(src).stem().string();
            bat << " \"" << objDir << "\\" << stem << ".obj\"";
        }
        bat << " >> \"" << logPath << "\" 2>&1\n";
        bat << "if errorlevel 1 (\n";
        bat << "    echo [HotReload] Link FAILED - see hotreload_compile.log\n";
        bat << "    exit /b 1\n";
        bat << ")\n\n";

        bat << "echo [HotReload] Build succeeded.\n";
    }

	// Run the batch file to compile and link GameCode for hot reload
    DBG("ProjectBuilder: Hot reload build (%d files) via cl.exe...", (int)sources.size());
    int result = system(batPath.c_str());

	// Check if hot reload is succeeded or failed
    if (result != 0)
    {
        DBG("ProjectBuilder: Hot reload build FAILED (code %d)", result);
        DBG("ProjectBuilder: See hotreload_compile.log for details");
        return false;
    }

    // Succeded
    DBG("ProjectBuilder: Hot reload build succeeded.");
    return true;
}