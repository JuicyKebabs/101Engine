#pragma once
#include <string>
//------------------------------------------------------------
// ClassTemplateGenerator class
// This utility class generates a pair of .h/.cpp files 
// with boilerplate code for a new class with the given name.
//------------------------------------------------------------

class ClassTemplateGenerator
{
public:
	static bool Generate(const std::string& className);
};