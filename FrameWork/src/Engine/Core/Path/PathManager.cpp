#include "PathManager.h"
#include "Engine/Core/Debug/Debug.h"

namespace fs = std::filesystem;

// Definitions of the static data members declared in PathManager.h.
// A single instance lives inside 101Framework.dll.
std::string PathManager::s_projectRoot;
bool PathManager::s_initialized = false;

// Initialize
bool PathManager::Initialize(const std::string& exepath)
{
	fs::path dir = fs::path(exepath).parent_path();	// Get directory path of .exe file

	while (true)
	{
		fs::path candidate = dir / "project.101";	// Candidate path for project.101 file

		// In case of finding project.101 file
		if(fs::exists(candidate))
		{
			s_projectRoot = dir.string();	// Set project root directory path
			s_initialized = true;			// Set initialization flag
			DBG("Project root directory found: %s", s_projectRoot.c_str());
			return true;
		}

		// In case of reaching the root directory without finding project.101 file
		fs::path parent = dir.parent_path();	// Get parent directory
		if (parent == dir)
		{
			DBG("Error: project.101 not found.");
			return false;
		}
		dir = parent;	// Move up to the parent directory
	}
}

std::string PathManager::Resolve(const std::string& relativePath)
{
	if (!s_initialized)
	{
		DBG("Error: PathManager is not initialized.");
		return relativePath;
	}
	return (fs::path(s_projectRoot) / relativePath).string();
}

std::wstring PathManager::ResolveW(const std::string& relativePath)
{
	std::string resolved = Resolve(relativePath);

	if (resolved.empty()) return std::wstring();
	int len = MultiByteToWideChar(CP_ACP, 0, resolved.c_str(), (int)resolved.size(), nullptr, 0);
	std::wstring result(len, 0);
	MultiByteToWideChar(CP_ACP, 0, resolved.c_str(), (int)resolved.size(), result.data(), len);
	return result;
}

std::string PathManager::GetProjectRoot()
{
	if (!s_initialized)
	{
		DBG("Error: PathManager is not initialized.");
		return "";
	}
	return s_projectRoot;
}

bool PathManager::IsInitialized()
{
	return s_initialized;
}