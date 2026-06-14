#pragma once
#include <string>

class BehaviorTemplateGenerator
{
public:
    // Generates ClassName.h and ClassName.cpp under Game/GameCode/.
    // Returns true on success, false if the class name is empty or a file
    // with that name already exists.
    static bool Generate(const std::string& className);
};
