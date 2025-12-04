/* Start Header *****************************************************************/
/*!
\file    EngineInterop_Math.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    20th November 2025
\brief
C API exports for managed C# scripting systems for math utilities and random number generation. Please
read the important note regarding CLS compliance for math functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "helpers/MathUtils.h"
#include "math/Vector2D.h"
#include "math/Vector3D.h"
#include <cmath>

// Export macro for C API
#ifdef _WIN32
    #ifdef BUILDING_ENGINE_INTEROP
        #define ENGINE_INTEROP_API extern "C" __declspec(dllexport)
    #else
        #define ENGINE_INTEROP_API extern "C" __declspec(dllimport)
    #endif
#else
    #define ENGINE_INTEROP_API extern "C"
#endif

// ============================================================================
// Math API - Random Number Generation
// ============================================================================

/// <summary>
/// Generate a random integer between min and max (inclusive)
/// </summary>
ENGINE_INTEROP_API int EngineInterop_Math_RandomInt(int min, int max) {
    return MathUtils::Randomize(min, max);
}

/// <summary>
/// Generate a random float between min and max
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_RandomFloat(float min, float max) {
    return MathUtils::Randomize(min, max);
}

/// <summary>
/// Generate a random integer with a specific seed
/// </summary>
ENGINE_INTEROP_API int EngineInterop_Math_RandomIntSeeded(int min, int max, unsigned int seed) {
    return MathUtils::Randomize(min, max, seed);
}

/// <summary>
/// Generate a random float with a specific seed
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_RandomFloatSeeded(float min, float max, unsigned int seed) {
    return MathUtils::Randomize(min, max, seed);
}

// ============================================================================
// Math API - Common Math Functions
// Note: C# Math class already provides many of these functions, but some of
// which could potentially be non-CLS compliant. It is safer to provide our own
// implementations to ensure consistent behavior across all scripting scenarios.
// https://learn.microsoft.com/en-us/dotnet/standard/clr#cls-compliance
// Example: https://learn.microsoft.com/en-us/dotnet/api/system.math.clamp?view=net-10.0
// Math.Clamp API is NOT CLS-compliant.
// ============================================================================

/// <summary>
/// Clamp a value between min and max
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Clamp(float value, float min, float max) {
    return MathUtils::Clamp(value, min, max);
}

/// <summary>
/// Clamp an integer value between min and max
/// </summary>
ENGINE_INTEROP_API int EngineInterop_Math_ClampInt(int value, int min, int max) {
    return MathUtils::Clamp(value, min, max);
}

/// <summary>
/// Linear interpolation between a and b by t (0-1)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

/// <summary>
/// Get the absolute value of a number
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Abs(float value) {
    return std::abs(value);
}

/// <summary>
/// Square root
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Sqrt(float value) {
    return std::sqrt(value);
}

/// <summary>
/// Power function
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Pow(float base, float exponent) {
    return std::pow(base, exponent);
}

/// <summary>
/// Round to nearest integer
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Round(float value) {
    return std::round(value);
}

/// <summary>
/// Round down to integer
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Floor(float value) {
    return std::floor(value);
}

/// <summary>
/// Round up to integer
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Ceil(float value) {
    return std::ceil(value);
}

/// <summary>
/// Get the minimum of two values
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Min(float a, float b) {
    return a < b ? a : b;
}

/// <summary>
/// Get the maximum of two values
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Max(float a, float b) {
    return a > b ? a : b;
}

// ============================================================================
// Math API - Trigonometry
// ============================================================================

/// <summary>
/// Sine (input in radians)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Sin(float radians) {
    return std::sin(radians);
}

/// <summary>
/// Cosine (input in radians)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Cos(float radians) {
    return std::cos(radians);
}

/// <summary>
/// Tangent (input in radians)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Tan(float radians) {
    return std::tan(radians);
}

/// <summary>
/// Arc sine (returns radians)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Asin(float value) {
    return std::asin(value);
}

/// <summary>
/// Arc cosine (returns radians)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Acos(float value) {
    return std::acos(value);
}

/// <summary>
/// Arc tangent (returns radians)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Atan(float value) {
    return std::atan(value);
}

/// <summary>
/// Arc tangent of y/x (returns radians, handles quadrants correctly)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Atan2(float y, float x) {
    return std::atan2(y, x);
}

/// <summary>
/// Convert degrees to radians
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_DegToRad(float degrees) {
    return degrees * 0.0174532925f; // PI / 180
}

/// <summary>
/// Convert radians to degrees
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_RadToDeg(float radians) {
    return radians * 57.2957795f; // 180 / PI
}

// ============================================================================
// Math API - Vector2D Operations
// ============================================================================

/// <summary>
/// Calculate the distance between two 2D points
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Distance2D(float x1, float y1, float x2, float y2) {
    return Vector2D::Distance(Vector2D(x1, y1), Vector2D(x2, y2));
}

/// <summary>
/// Calculate the squared distance between two 2D points (faster, no sqrt)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_DistanceSquared2D(float x1, float y1, float x2, float y2) {
    return Vector2D::SquareDistance(Vector2D(x1, y1), Vector2D(x2, y2));
}

/// <summary>
/// Calculate the length/magnitude of a 2D vector
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Length2D(float x, float y) {
    return Vector2D(x, y).Length();
}

/// <summary>
/// Calculate the squared length of a 2D vector (faster, no sqrt)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_LengthSquared2D(float x, float y) {
    return Vector2D(x, y).SquareLength();
}

/// <summary>
/// Normalize a 2D vector and return the result
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Math_Normalize2D(float x, float y, float* outX, float* outY) {
    Vector2D normalized = Vector2D(x, y).Normalized();
    if (outX) *outX = normalized.X;
    if (outY) *outY = normalized.Y;
}

/// <summary>
/// Calculate the dot product of two 2D vectors
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Dot2D(float x1, float y1, float x2, float y2) {
    return Vector2D::Dot(Vector2D(x1, y1), Vector2D(x2, y2));
}

/// <summary>
/// Calculate the 2D cross product (scalar result)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_Cross2D(float x1, float y1, float x2, float y2) {
    return Vector2D::Cross(Vector2D(x1, y1), Vector2D(x2, y2));
}

/// <summary>
/// Linear interpolation between two 2D vectors
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Math_Lerp2D(float x1, float y1, float x2, float y2, float t, float* outX, float* outY) {
    Vector2D result = Vector2D::Lerp(Vector2D(x1, y1), Vector2D(x2, y2), t);
    if (outX) *outX = result.X;
    if (outY) *outY = result.Y;
}

/// <summary>
/// Clamp a 2D vector between min and max bounds
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Math_ClampVector2D(float x, float y, float minX, float minY, float maxX, float maxY, float* outX, float* outY) {
    Vector2D result = Vector2D::ClampVector(Vector2D(x, y), Vector2D(minX, minY), Vector2D(maxX, maxY));
    if (outX) *outX = result.X;
    if (outY) *outY = result.Y;
}

// ============================================================================
// Math API - Angle and Direction
// ============================================================================

/// <summary>
/// Calculate the angle (in radians) from one point to another
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Math_AngleTo(float fromX, float fromY, float toX, float toY) {
    return std::atan2(toY - fromY, toX - fromX);
}

/// <summary>
/// Create a direction vector from an angle (in radians)
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Math_DirectionFromAngle(float radians, float* outX, float* outY) {
    if (outX) *outX = std::cos(radians);
    if (outY) *outY = std::sin(radians);
}

/// <summary>
/// Rotate a 2D vector by an angle (in radians)
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Math_Rotate2D(float x, float y, float radians, float* outX, float* outY) {
    float cosAngle = std::cos(radians);
    float sinAngle = std::sin(radians);
    
    if (outX) *outX = x * cosAngle - y * sinAngle;
    if (outY) *outY = x * sinAngle + y * cosAngle;
}
