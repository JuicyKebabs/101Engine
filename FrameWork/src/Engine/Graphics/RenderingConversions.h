#pragma once

#include "Engine/Core/Math/Math.h"
#include "Renderer/Math.h"

#include <cmath>
#include <algorithm>

namespace EngineRendering
{

// sqrt(||A||_1 * ||A||_infinity) bounds the largest singular value.
// Unlike the Frobenius bound it preserves the exact unit/diagonal scale used
// for fitting a model into a RectTransform, while remaining safe for shear.
inline float ConservativeLinearScale(const Matrix4x4& matrix)
{
    double maximumRowSum = 0;
    double maximumColumnSum = 0;

    for (std::size_t index = 0; index < 3; ++index)
    {
        double rowSum = 0;
        double columnSum = 0;

        for (std::size_t component = 0; component < 3; ++component)
        {
            rowSum += std::abs(static_cast<double>(matrix.m[index][component]));
            columnSum += std::abs(static_cast<double>(matrix.m[component][index]));
        }

        maximumRowSum = (std::max)(maximumRowSum, rowSum);
        maximumColumnSum = (std::max)(maximumColumnSum, columnSum);
    }

    return static_cast<float>(std::sqrt(maximumRowSum * maximumColumnSum));
}

// Engine and Renderer keep independent value types. Copy values at this boundary
// rather than aliasing unrelated structs or exporting either side's GPU objects.
inline rendering::Vector3 ToRenderer(const Vector3& source)
{
    rendering::Vector3 value;
    value.x = source.x;
    value.y = source.y;
    value.z = source.z;
    return value;
}

inline rendering::Vector4 ToRenderer(const Vector4& source)
{
    rendering::Vector4 value;
    value.x = source.x;
    value.y = source.y;
    value.z = source.z;
    value.w = source.w;
    return value;
}

inline Vector3 FromRenderer(const rendering::Vector3& source)
{
    Vector3 value;
    value.x = source.x;
    value.y = source.y;
    value.z = source.z;
    return value;
}

inline Matrix4x4 FromRenderer(const rendering::Matrix4x4& source)
{
    Matrix4x4 value;
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            value.m[row][column] = source.m[row][column];
        }
    }

    return value;
}

inline rendering::Matrix4x4 ToRenderer(const Matrix4x4& source)
{
    rendering::Matrix4x4 value;
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t column = 0; column < 4; ++column)
        {
            value.m[row][column] = source.m[row][column];
        }
    }

    return value;
}

// These transfer functions describe the Editor/storage boundary. Lighting and
// Component setters already receive linear values; Alpha is never transformed.
inline float SrgbToLinear(float channel)
{
    if (channel <= 0.04045f)
    {
        return channel / 12.92f;
    }

    return std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

inline float LinearToSrgb(float channel)
{
    if (channel <= 0.0031308f)
    {
        return channel * 12.92f;
    }

    return 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
}

inline Vector4 SrgbToLinear(const Vector4& source)
{
    Vector4 result = source;
    result.x = SrgbToLinear(source.x);
    result.y = SrgbToLinear(source.y);
    result.z = SrgbToLinear(source.z);
    return result;
}

inline Vector4 LinearToSrgb(const Vector4& source)
{
    Vector4 result = source;
    result.x = LinearToSrgb(source.x);
    result.y = LinearToSrgb(source.y);
    result.z = LinearToSrgb(source.z);
    return result;
}

}
