#pragma once
#include <string>
//-----------------------------------------------------------------------
// BehaviorTemplateGenerator class
// This utility class generates a pair of .h/.cpp files with boilerplate
// code for a new Behavior subclass with the given name, and places them
// under Game/GameCode/.
//-----------------------------------------------------------------------

class BehaviorTemplateGenerator
{
public:
    // Generates ClassName.h and ClassName.cpp under Game/GameCode/.
    // Returns true on success, false if the class name is empty or a file
    // with that name already exists.
    static bool Generate(const std::string& className);
};
