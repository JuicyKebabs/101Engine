#include "ClassTemplateGenerator.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <fstream>
#include <filesystem>

bool ClassTemplateGenerator::Generate(const std::string& className)
{
    if (className.empty())
    {
        DBG("ClassTemplateGenerator: Class name is empty.");
        return false;
    }

    // Directory and file paths
    std::string baseDir = PathManager::Resolve("Game/GameCode");
    std::string headerPath = baseDir + "/" + className + ".h";
    std::string sourcePath = baseDir + "/" + className + ".cpp";

	// Check for existing files
    if (std::filesystem::exists(headerPath) || std::filesystem::exists(sourcePath))
    {
        DBG("ClassTemplateGenerator: File already exists for '%s'", className.c_str());
        return false;
    }

    // Make sure the destination directory exists
    std::filesystem::create_directories(baseDir);

    // Generate .h
    {
        std::ofstream header(headerPath);

        if (!header.is_open())
        {
            DBG("ClassTemplateGenerator: Failed to create %s", headerPath.c_str());
            return false;
        }

        header << "#pragma once\n\n";
        header << "class " << className << "\n";
        header << "{\n";
        header << "public:\n";
        header << "    " << className << "();\n";
        header << "    ~" << className << "();\n";
        header << "};\n";
    }
    // Generate .cpp
    {
        std::ofstream source(sourcePath);
        if (!source.is_open())
        {
            DBG("ClassTemplateGenerator: Failed to create %s", sourcePath.c_str());
            return false;
        }

        source << "#include \"" << className << ".h\"\n\n";
        source << className << "::" << className << "()\n";
        source << "{\n}\n\n";
        source << className << "::~" << className << "()\n";
        source << "{\n}\n";
    }

    DBG("ClassTemplateGenerator: Generated %s.h / .cpp", className.c_str());
    return true;
}