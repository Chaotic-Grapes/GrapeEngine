/* Start Header *****************************************************************/
/*!
\file    Interop_Math.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    20th November 2025
\brief
C API exports for managed C# scripting systems for math utilities and random
number generation. Please read the important note regarding CLS compliance for
math functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "helpers/MathUtils.h"
#include "math/Vector2D.h"
#include "math/Vector3D.h"
#include <cmath>

// ============================================================================
// Math API - Random Number Generation
// ============================================================================

/**
 * @brief Generate a random integer between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return int Random integer in [min, max]
 */
INTEROP_API int EngineInterop_Math_RandomInt(int min, int max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random byte between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return unsigned char Random byte in [min, max]
 */
INTEROP_API unsigned char EngineInterop_Math_RandomByte(unsigned char min, unsigned char max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random signed byte between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return signed char Random signed byte in [min, max]
 */
INTEROP_API signed char EngineInterop_Math_RandomSByte(signed char min, signed char max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random short between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return short Random short in [min, max]
 */
INTEROP_API short EngineInterop_Math_RandomShort(short min, short max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random unsigned short between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return unsigned short Random value in [min, max]
 */
INTEROP_API unsigned short EngineInterop_Math_RandomUShort(unsigned short min, unsigned short max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random unsigned integer between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return unsigned int Random value in [min, max]
 */
INTEROP_API unsigned int EngineInterop_Math_RandomUInt(unsigned int min, unsigned int max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random long between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return long Random value in [min, max]
 */
INTEROP_API long EngineInterop_Math_RandomLong(long min, long max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random unsigned long between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return unsigned long Random value in [min, max]
 */
INTEROP_API unsigned long EngineInterop_Math_RandomULong(unsigned long min, unsigned long max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random 64-bit long between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return long long Random value in [min, max]
 */
INTEROP_API long long EngineInterop_Math_RandomLong64(long long min, long long max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random unsigned 64-bit long between min and max (inclusive)
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @return unsigned long long Random value in [min, max]
 */
INTEROP_API unsigned long long EngineInterop_Math_RandomULong64(unsigned long long min, unsigned long long max) {
    return MathUtils::Randomize(min, max);
}

/**
 * @brief Generate a random float between min and max
 *
 * @param min Lower bound
 * @param max Upper bound
 * @return float Random float in [min, max]
 */
INTEROP_API float EngineInterop_Math_RandomFloat(float min, float max) {
    return MathUtils::Randomize(min, max);
}

// ============================================================================
// Math API - Random Number Generation (Seeded)
// ============================================================================

/**
 * @brief Generate a random integer with a specific seed
 *
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive)
 * @param seed Seed value to use
 * @return int Random integer in [min, max] generated with seed
 */
INTEROP_API int EngineInterop_Math_RandomIntSeeded(int min, int max, unsigned int seed) {
    return MathUtils::Randomize(min, max, seed);
}

/**
 * @brief Generate a random float with a specific seed
 *
 * @param min Lower bound
 * @param max Upper bound
 * @param seed Seed value to use
 * @return float Random float in [min, max] generated with seed
 */
INTEROP_API float EngineInterop_Math_RandomFloatSeeded(float min, float max, unsigned int seed) {
    return MathUtils::Randomize(min, max, seed);
}

// ============================================================================
// Math API - Clamping
// Note: C# Math class already provides many of these functions, but some
// could potentially be non-CLS compliant. We provide our own implementations
// to ensure consistent behavior across all scripting scenarios.
// https://learn.microsoft.com/en-us/dotnet/standard/clr#cls-compliance
// ============================================================================

/**
 * @brief Clamp a float value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return float Clamped value
 */
INTEROP_API float EngineInterop_Math_Clamp(float value, float min, float max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp an integer value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return int Clamped value
 */
INTEROP_API int EngineInterop_Math_ClampInt(int value, int min, int max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp a byte value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return unsigned char Clamped value
 */
INTEROP_API unsigned char EngineInterop_Math_ClampByte(unsigned char value, unsigned char min, unsigned char max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp a signed byte value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return signed char Clamped value
 */
INTEROP_API signed char EngineInterop_Math_ClampSByte(signed char value, signed char min, signed char max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp a short value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return short Clamped value
 */
INTEROP_API short EngineInterop_Math_ClampShort(short value, short min, short max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp an unsigned short value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return unsigned short Clamped value
 */
INTEROP_API unsigned short EngineInterop_Math_ClampUShort(unsigned short value, unsigned short min, unsigned short max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp an unsigned integer value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return unsigned int Clamped value
 */
INTEROP_API unsigned int EngineInterop_Math_ClampUInt(unsigned int value, unsigned int min, unsigned int max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp a long value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return long Clamped value
 */
INTEROP_API long EngineInterop_Math_ClampLong(long value, long min, long max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp an unsigned long value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return unsigned long Clamped value
 */
INTEROP_API unsigned long EngineInterop_Math_ClampULong(unsigned long value, unsigned long min, unsigned long max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp a 64-bit long value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return long long Clamped value
 */
INTEROP_API long long EngineInterop_Math_ClampLong64(long long value, long long min, long long max) {
    return MathUtils::Clamp(value, min, max);
}

/**
 * @brief Clamp an unsigned 64-bit long value between min and max
 *
 * @param value Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return unsigned long long Clamped value
 */
INTEROP_API unsigned long long EngineInterop_Math_ClampULong64(unsigned long long value, unsigned long long min, unsigned long long max) {
    return MathUtils::Clamp(value, min, max);
}

// ============================================================================
// Math API - Basic Operations
// ============================================================================

/**
 * @brief Linear interpolation between a and b by t (0-1)
 *
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (0..1)
 * @return float Interpolated value
 */
INTEROP_API float EngineInterop_Math_Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

/**
 * @brief Get the absolute value of a float
 *
 * @param value Input value
 * @return float Absolute value
 */
INTEROP_API float EngineInterop_Math_Abs(float value) {
    return std::abs(value);
}

/**
 * @brief Get the square root
 *
 * @param value Input value
 * @return float Square root of value
 */
INTEROP_API float EngineInterop_Math_Sqrt(float value) {
    return std::sqrt(value);
}

/**
 * @brief Raise base to the power of exponent
 *
 * @param base Base value
 * @param exponent Exponent value
 * @return float Result of base^exponent
 */
INTEROP_API float EngineInterop_Math_Pow(float base, float exponent) {
    return std::pow(base, exponent);
}

/**
 * @brief Round to nearest integer
 *
 * @param value Input value
 * @return float Rounded value
 */
INTEROP_API float EngineInterop_Math_Round(float value) {
    return std::round(value);
}

/**
 * @brief Round down to nearest integer
 *
 * @param value Input value
 * @return float Floored value
 */
INTEROP_API float EngineInterop_Math_Floor(float value) {
    return std::floor(value);
}

/**
 * @brief Round up to nearest integer
 *
 * @param value Input value
 * @return float Ceiled value
 */
INTEROP_API float EngineInterop_Math_Ceil(float value) {
    return std::ceil(value);
}

// ============================================================================
// Math API - Min/Max Operations
// ============================================================================

/**
 * @brief Get the minimum of two floats
 *
 * @param a First value
 * @param b Second value
 * @return float The smaller of a and b
 */
INTEROP_API float EngineInterop_Math_Min(float a, float b) {
    return a < b ? a : b;
}

/**
 * @brief Get the maximum of two floats
 *
 * @param a First value
 * @param b Second value
 * @return float The larger of a and b
 */
INTEROP_API float EngineInterop_Math_Max(float a, float b) {
    return a > b ? a : b;
}

/**
 * @brief Get the minimum of two integers
 *
 * @param a First value
 * @param b Second value
 * @return int The smaller of a and b
 */
INTEROP_API int EngineInterop_Math_MinInt(int a, int b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two integers
 *
 * @param a First value
 * @param b Second value
 * @return int The larger of a and b
 */
INTEROP_API int EngineInterop_Math_MaxInt(int a, int b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two bytes
 *
 * @param a First value
 * @param b Second value
 * @return unsigned char The smaller of a and b
 */
INTEROP_API unsigned char EngineInterop_Math_MinByte(unsigned char a, unsigned char b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two bytes
 *
 * @param a First value
 * @param b Second value
 * @return unsigned char The larger of a and b
 */
INTEROP_API unsigned char EngineInterop_Math_MaxByte(unsigned char a, unsigned char b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two signed bytes
 *
 * @param a First value
 * @param b Second value
 * @return signed char The smaller of a and b
 */
INTEROP_API signed char EngineInterop_Math_MinSByte(signed char a, signed char b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two signed bytes
 *
 * @param a First value
 * @param b Second value
 * @return signed char The larger of a and b
 */
INTEROP_API signed char EngineInterop_Math_MaxSByte(signed char a, signed char b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two shorts
 *
 * @param a First value
 * @param b Second value
 * @return short The smaller of a and b
 */
INTEROP_API short EngineInterop_Math_MinShort(short a, short b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two shorts
 *
 * @param a First value
 * @param b Second value
 * @return short The larger of a and b
 */
INTEROP_API short EngineInterop_Math_MaxShort(short a, short b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two unsigned shorts
 *
 * @param a First value
 * @param b Second value
 * @return unsigned short The smaller of a and b
 */
INTEROP_API unsigned short EngineInterop_Math_MinUShort(unsigned short a, unsigned short b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two unsigned shorts
 *
 * @param a First value
 * @param b Second value
 * @return unsigned short The larger of a and b
 */
INTEROP_API unsigned short EngineInterop_Math_MaxUShort(unsigned short a, unsigned short b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two unsigned integers
 *
 * @param a First value
 * @param b Second value
 * @return unsigned int The smaller of a and b
 */
INTEROP_API unsigned int EngineInterop_Math_MinUInt(unsigned int a, unsigned int b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two unsigned integers
 *
 * @param a First value
 * @param b Second value
 * @return unsigned int The larger of a and b
 */
INTEROP_API unsigned int EngineInterop_Math_MaxUInt(unsigned int a, unsigned int b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two longs
 *
 * @param a First value
 * @param b Second value
 * @return long The smaller of a and b
 */
INTEROP_API long EngineInterop_Math_MinLong(long a, long b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two longs
 *
 * @param a First value
 * @param b Second value
 * @return long The larger of a and b
 */
INTEROP_API long EngineInterop_Math_MaxLong(long a, long b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two unsigned longs
 *
 * @param a First value
 * @param b Second value
 * @return unsigned long The smaller of a and b
 */
INTEROP_API unsigned long EngineInterop_Math_MinULong(unsigned long a, unsigned long b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two unsigned longs
 *
 * @param a First value
 * @param b Second value
 * @return unsigned long The larger of a and b
 */
INTEROP_API unsigned long EngineInterop_Math_MaxULong(unsigned long a, unsigned long b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two 64-bit longs
 *
 * @param a First value
 * @param b Second value
 * @return long long The smaller of a and b
 */
INTEROP_API long long EngineInterop_Math_MinLong64(long long a, long long b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two 64-bit longs
 *
 * @param a First value
 * @param b Second value
 * @return long long The larger of a and b
 */
INTEROP_API long long EngineInterop_Math_MaxLong64(long long a, long long b) {
    return MathUtils::Max(a, b);
}

/**
 * @brief Get the minimum of two unsigned 64-bit longs
 *
 * @param a First value
 * @param b Second value
 * @return unsigned long long The smaller of a and b
 */
INTEROP_API unsigned long long EngineInterop_Math_MinULong64(unsigned long long a, unsigned long long b) {
    return MathUtils::Min(a, b);
}

/**
 * @brief Get the maximum of two unsigned 64-bit longs
 *
 * @param a First value
 * @param b Second value
 * @return unsigned long long The larger of a and b
 */
INTEROP_API unsigned long long EngineInterop_Math_MaxULong64(unsigned long long a, unsigned long long b) {
    return MathUtils::Max(a, b);
}

// ============================================================================
// Math API - Trigonometry
// ============================================================================

/**
 * @brief Sine (input in radians)
 *
 * @param radians Angle in radians
 * @return float Sine of the angle
 */
INTEROP_API float EngineInterop_Math_Sin(float radians) {
    return std::sin(radians);
}

/**
 * @brief Cosine (input in radians)
 *
 * @param radians Angle in radians
 * @return float Cosine of the angle
 */
INTEROP_API float EngineInterop_Math_Cos(float radians) {
    return std::cos(radians);
}

/**
 * @brief Tangent (input in radians)
 *
 * @param radians Angle in radians
 * @return float Tangent of the angle
 */
INTEROP_API float EngineInterop_Math_Tan(float radians) {
    return std::tan(radians);
}

/**
 * @brief Arc sine (returns radians)
 *
 * @param value Input value
 * @return float Arc sine in radians
 */
INTEROP_API float EngineInterop_Math_Asin(float value) {
    return std::asin(value);
}

/**
 * @brief Arc cosine (returns radians)
 *
 * @param value Input value
 * @return float Arc cosine in radians
 */
INTEROP_API float EngineInterop_Math_Acos(float value) {
    return std::acos(value);
}

/**
 * @brief Arc tangent (returns radians)
 *
 * @param value Input value
 * @return float Arc tangent in radians
 */
INTEROP_API float EngineInterop_Math_Atan(float value) {
    return std::atan(value);
}

/**
 * @brief Arc tangent of y/x (returns radians, handles quadrants correctly)
 *
 * @param y Y component
 * @param x X component
 * @return float Angle in radians
 */
INTEROP_API float EngineInterop_Math_Atan2(float y, float x) {
    return std::atan2(y, x);
}

/**
 * @brief Convert degrees to radians
 *
 * @param degrees Angle in degrees
 * @return float Angle in radians
 */
INTEROP_API float EngineInterop_Math_DegToRad(float degrees) {
    return degrees * 0.0174532925f; // PI / 180
}

/**
 * @brief Convert radians to degrees
 *
 * @param radians Angle in radians
 * @return float Angle in degrees
 */
INTEROP_API float EngineInterop_Math_RadToDeg(float radians) {
    return radians * 57.2957795f; // 180 / PI
}

// ============================================================================
// Math API - Vector2D Operations
// ============================================================================

/**
 * @brief Calculate the distance between two 2D points
 *
 * @param x1 X of first point
 * @param y1 Y of first point
 * @param x2 X of second point
 * @param y2 Y of second point
 * @return float Distance between points
 */
INTEROP_API float EngineInterop_Math_Distance2D(float x1, float y1, float x2, float y2) {
    return Vector2D::Distance(Vector2D(x1, y1), Vector2D(x2, y2));
}

/**
 * @brief Calculate the squared distance between two 2D points (faster, no sqrt)
 *
 * @param x1 X of first point
 * @param y1 Y of first point
 * @param x2 X of second point
 * @param y2 Y of second point
 * @return float Squared distance between points
 */
INTEROP_API float EngineInterop_Math_DistanceSquared2D(float x1, float y1, float x2, float y2) {
    return Vector2D::SquareDistance(Vector2D(x1, y1), Vector2D(x2, y2));
}

/**
 * @brief Calculate the length/magnitude of a 2D vector
 *
 * @param x X component
 * @param y Y component
 * @return float Length of the vector
 */
INTEROP_API float EngineInterop_Math_Length2D(float x, float y) {
    return Vector2D(x, y).Length();
}

/**
 * @brief Calculate the squared length of a 2D vector (faster, no sqrt)
 *
 * @param x X component
 * @param y Y component
 * @return float Squared length of the vector
 */
INTEROP_API float EngineInterop_Math_LengthSquared2D(float x, float y) {
    return Vector2D(x, y).SquareLength();
}

/**
 * @brief Normalize a 2D vector and return the result
 *
 * @param x X component
 * @param y Y component
 * @param outX Pointer to receive normalized X (may be null)
 * @param outY Pointer to receive normalized Y (may be null)
 */
INTEROP_API void EngineInterop_Math_Normalize2D(float x, float y, float* outX, float* outY) {
    Vector2D normalized = Vector2D(x, y).Normalized();
    if (outX) *outX = normalized.X;
    if (outY) *outY = normalized.Y;
}

/**
 * @brief Calculate the dot product of two 2D vectors
 *
 * @param x1 X of first vector
 * @param y1 Y of first vector
 * @param x2 X of second vector
 * @param y2 Y of second vector
 * @return float Dot product
 */
INTEROP_API float EngineInterop_Math_Dot2D(float x1, float y1, float x2, float y2) {
    return Vector2D::Dot(Vector2D(x1, y1), Vector2D(x2, y2));
}

/**
 * @brief Calculate the 2D cross product (scalar result)
 *
 * @param x1 X of first vector
 * @param y1 Y of first vector
 * @param x2 X of second vector
 * @param y2 Y of second vector
 * @return float Cross product (scalar)
 */
INTEROP_API float EngineInterop_Math_Cross2D(float x1, float y1, float x2, float y2) {
    return Vector2D::Cross(Vector2D(x1, y1), Vector2D(x2, y2));
}

/**
 * @brief Linear interpolation between two 2D vectors
 *
 * @param x1 X of start vector
 * @param y1 Y of start vector
 * @param x2 X of end vector
 * @param y2 Y of end vector
 * @param t Interpolation factor (0..1)
 * @param outX Pointer to receive X of result
 * @param outY Pointer to receive Y of result
 */
INTEROP_API void EngineInterop_Math_Lerp2D(float x1, float y1, float x2, float y2, float t, float* outX, float* outY) {
    Vector2D result = Vector2D::Lerp(Vector2D(x1, y1), Vector2D(x2, y2), t);
    if (outX) *outX = result.X;
    if (outY) *outY = result.Y;
}

/**
 * @brief Clamp a 2D vector between min and max bounds
 *
 * @param x X component
 * @param y Y component
 * @param minX Minimum X
 * @param minY Minimum Y
 * @param maxX Maximum X
 * @param maxY Maximum Y
 * @param outX Pointer to receive clamped X
 * @param outY Pointer to receive clamped Y
 */
INTEROP_API void EngineInterop_Math_ClampVector2D(float x, float y, float minX, float minY, float maxX, float maxY, float* outX, float* outY) {
    Vector2D result = Vector2D::ClampVector(Vector2D(x, y), Vector2D(minX, minY), Vector2D(maxX, maxY));
    if (outX) *outX = result.X;
    if (outY) *outY = result.Y;
}

// ============================================================================
// Math API - Angle and Direction
// ============================================================================

/**
 * @brief Calculate the angle (in radians) from one point to another
 *
 * @param fromX X of origin point
 * @param fromY Y of origin point
 * @param toX X of target point
 * @param toY Y of target point
 * @return float Angle in radians
 */
INTEROP_API float EngineInterop_Math_AngleTo(float fromX, float fromY, float toX, float toY) {
    return std::atan2(toY - fromY, toX - fromX);
}

/**
 * @brief Create a direction vector from an angle (in radians)
 *
 * @param radians Angle in radians
 * @param outX Pointer to receive X component
 * @param outY Pointer to receive Y component
 */
INTEROP_API void EngineInterop_Math_DirectionFromAngle(float radians, float* outX, float* outY) {
    if (outX) *outX = std::cos(radians);
    if (outY) *outY = std::sin(radians);
}

/**
 * @brief Rotate a 2D vector by an angle (in radians)
 *
 * @param x X component
 * @param y Y component
 * @param radians Rotation angle in radians
 * @param outX Pointer to receive rotated X
 * @param outY Pointer to receive rotated Y
 */
INTEROP_API void EngineInterop_Math_Rotate2D(float x, float y, float radians, float* outX, float* outY) {
    float cosAngle = std::cos(radians);
    float sinAngle = std::sin(radians);
    
    if (outX) *outX = x * cosAngle - y * sinAngle;
    if (outY) *outY = x * sinAngle + y * cosAngle;
}
