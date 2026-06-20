#include "BehaviorTemplateGenerator.h"
#include "Engine/Core/Path/PathManager.h"
#include "Engine/Core/Debug/Debug.h"
#include <fstream>
#include <filesystem>

bool BehaviorTemplateGenerator::Generate(const std::string& className)
{
    if (className.empty())
    {
        DBG("BehaviorTemplateGenerator: Class name is empty.");
        return false;
    }

	// Directory and file paths
    std::string baseDir = PathManager::Resolve("Game/GameCode");
    std::string headerPath = baseDir + "/" + className + ".h";
    std::string sourcePath = baseDir + "/" + className + ".cpp";

    // Check for existing files
    if (std::filesystem::exists(headerPath) || std::filesystem::exists(sourcePath))
    {
        DBG("BehaviorTemplateGenerator: File already exists for '%s'", className.c_str());
        return false;
    }

    // Make sure the destination directory exists
    std::filesystem::create_directories(baseDir);

    // Generate .h
    {
        std::ofstream header(headerPath);
        if (!header.is_open())
        {
            DBG("BehaviorTemplateGenerator: Failed to create %s", headerPath.c_str());
            return false;
        }

        header << "#pragma once\n";
        header << "#include \"Engine/Component/Behavior.h\"\n\n";
        header << "class " << className << " : public Behavior\n";
        header << "{\n";
        header << "public:\n";
        header << "    void Start() override;\n";
        header << "    void PreUpdate() override;\n";
        header << "    void Update() override;\n";
        header << "    void LateUpdate() override;\n";
        header << "    void Destroy() override;\n";
        header << "};\n";
    }

    // Generate .cpp
    {
        std::ofstream source(sourcePath);
        if (!source.is_open())
        {
            DBG("BehaviorTemplateGenerator: Failed to create %s", sourcePath.c_str());
            return false;
        }

        source << "#include \"" << className << ".h\"\n";
        source << "#include \"Engine/Scene/ComponentRegistry.h\"\n\n";
        source << "REGISTER_GAME_COMPONENT(" << className << ")\n\n";
        source << "void " << className << "::Start() {}\n";
        source << "void " << className << "::PreUpdate() {}\n";
        source << "void " << className << "::Update() {}\n";
        source << "void " << className << "::LateUpdate() {}\n";
        source << "void " << className << "::Destroy() {}\n";
    }

    DBG("BehaviorTemplateGenerator: Generated %s.h / .cpp", className.c_str());
    return true;
}
