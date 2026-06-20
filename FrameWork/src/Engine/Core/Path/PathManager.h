#pragma once
#include <string>
#include <filesystem>
//----------------------------------------------------------------------------------------------------------------------
// PathManager class
// This class is responsible for managing file paths in the project.
// All function's definitions are in PathManager.cpp to make sure other dlls can access the original data (not a copy)
//----------------------------------------------------------------------------------------------------------------------

class PathManager
{
public:
	// Find project.101 file from executable path and return its directory path
	// If the file is not found, return false;
	static bool Initialize(const std::string& exepath);

	// Resolve relative path to absolute path
	static std::string Resolve(const std::string& relativePath);

	// Getters
	static std::string GetProjectRoot();
	static bool IsInitialized();

private:
	static std::string s_projectRoot;	// Project root directory path
	static bool s_initialized;			// Initialization flag
};