#pragma once

#include <array>

namespace rendering
{

struct DirectionalLight
{
	std::array<float, 4> lightDirection{ 0, -1, 0, 0 };
	std::array<float, 4> lightColor{ 1, 1, 1, 1 };
	std::array<float, 4> ambient{ 0.15f, 0.15f, 0.15f, 0 };
};

struct PointLight
{
	std::array<float, 4> lightPosition{ 0, 0, 0, 1 };
	std::array<float, 4> lightColor{ 1, 1, 1, 1 };
	std::array<float, 4> attenuation{ 1.0f, 0.09f, 0.032f, 0 }; // constant, linear, quadratic
};

struct SpotLight
{
	std::array<float, 4> lightPosition{ 0, 0, 0, 1 };
	std::array<float, 4> lightDirection{ 0, -1, 0, 0 };
	std::array<float, 4> lightColor{ 1, 1, 1, 1 };
	std::array<float, 4> attenuation{ 1.0f, 0.09f, 0.032f, 0 }; // constant, linear, quadratic
	float innerCutoff = 12.5f; // in degrees
	float outerCutoff = 17.5f; // in degrees
};

}