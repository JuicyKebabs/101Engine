#pragma once
#include <string>
#include <filesystem>

class PathManager
{
public:
	// Find project.101 file from executable path and return its directory path
	// If the file is not found, return false;
	static bool Initialize(const std::string& exepath);

	// Resolve relative path to absolute path
	static std::string Resolve(const std::string& relativePath);

	static std::string GetProjectRoot() { return s_projectRoot; }
	static bool IsInitialized() { return s_initialized; }

private:
	static inline  std::string s_projectRoot;	// Project root directory path
	static inline  bool s_initialized = false;	// Initialization flag
};