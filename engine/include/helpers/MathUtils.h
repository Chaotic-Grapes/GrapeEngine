/* Start Header *****************************************************************/
/*!
\file   MathUtils.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Declares a collection of mathematical utility functions and templates.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef MATHHELPER_H
#define MATHHELPER_H

#include <random>
#include "math/Vector2D.h"
#include "math/Vector3D.h"
#include "math/Vector4D.h"

class MathUtils {
public:
    /**
     * @brief Return a uniformly distributed random value in [minVal, maxVal].
     * @tparam T Integral or floating-point type.
     * @param minVal Lower bound of the range.
     * @param maxVal Upper bound of the range.
     * @param seed Optional seed; 0 uses a random device seed.
     * @return Random value in [minVal, maxVal].
     */
    template <typename T>
    static T Randomize(T minVal, T maxVal, const unsigned seed = 0) {
        // Seed
        thread_local std::mt19937 engine;
        if (seed != 0)
            engine.seed(seed);
        else {
            // Default seed otherwise
            thread_local std::random_device rd;
            engine.seed(rd());
        }

        if constexpr (std::is_integral_v<T>) {
            // std::uniform_int_distribution does not accept char/byte-sized types
            // as template parameters on some standard library implementations.
            // Use a wider integer type for the distribution and cast back.
            using DistT = std::conditional_t<(sizeof(T) < sizeof(int)), int, T>;
            std::uniform_int_distribution<DistT> dist(static_cast<DistT>(minVal), static_cast<DistT>(maxVal));
            return static_cast<T>(dist(engine));
        }
        else if constexpr (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> dist(minVal, maxVal);
            return dist(engine);
        }
		else return 0; // Unsupported type
    }

    /**
     * @brief Linearly interpolate between two values.
     * @tparam T Numeric or vector type supporting addition and scalar multiplication.
     * @param a Start value (t = 0).
     * @param b End value (t = 1).
     * @param t Interpolation factor.
     * @return Interpolated value a + (b - a) * t.
     */
    template <typename T>
    static T Lerp(T a, T b, float t) {
        return a + (b - a) * t;
    }

    /**
     * @brief Clamp a value within [minVal, maxVal].
     * @tparam T Comparable type.
     * @param value Value to clamp.
     * @param minVal Lower bound.
     * @param maxVal Upper bound.
     * @return Clamped value.
     */
    template <typename T>
    static T Clamp(T value, T minVal, T maxVal) {
        return value < minVal 
            ? minVal 
            : (value > maxVal ? maxVal : value);
    }

    /**
     * @brief Return the smaller of two values.
     * @tparam T Comparable type.
     * @param a First value.
     * @param b Second value.
     * @return The smaller of a and b.
     */
    template <typename T>
    static T Min(T a, T b) {
        return a < b ? a : b;
    }

    /**
     * @brief Return the larger of two values.
     * @tparam T Comparable type.
     * @param a First value.
     * @param b Second value.
     * @return The larger of a and b.
     */
    template <typename T>
    static T Max(T a, T b) {
        return a > b ? a : b;
    }

    /**
     * @brief Return the absolute value of an integral or floating-point value.
     * @tparam T Integral or floating-point type.
     * @param value Input value.
     * @return Non-negative magnitude of value.
     */
    template <typename T>
    static T Abs(T value) {
        if constexpr (std::is_integral_v<T>) {
            return value < 0 ? -value : value;
        }
        else if constexpr (std::is_floating_point_v<T>) {
            return std::abs(value);
        }
        else {
            return value;
        }
    }

    /**
     * @brief Promote a Vector2D to a Vector3D by appending a Z component.
     * @param vec2 Source 2D vector.
     * @param z Value for the Z component (default 0).
     * @return 3D vector (vec2.X, vec2.Y, z).
     */
    static Vector3D ToVector3D(const Vector2D& vec2, float z = 0.0f) {
        return Vector3D(vec2.X, vec2.Y, z);
    }

    /**
     * @brief Promote a Vector2D to a Vector4D by appending Z and W components.
     * @param vec2 Source 2D vector.
     * @param z Value for the Z component (default 0).
     * @param w Value for the W component (default 1, denoting a point).
     * @return 4D vector (vec2.X, vec2.Y, z, w).
     */
    static Vector4D ToVector4D(const Vector2D& vec2, float z = 0.0f, float w = 1.0f) {
        return Vector4D(vec2.X, vec2.Y, z, w);
    }

    /**
     * @brief Truncate a Vector3D to a Vector2D by dropping the Z component.
     * @param vec3 Source 3D vector.
     * @return 2D vector (vec3.X, vec3.Y).
     */
    static Vector2D ToVector2D(const Vector3D& vec3) {
        return Vector2D(vec3.X, vec3.Y);
    }

    /**
     * @brief Truncate a Vector4D to a Vector2D by dropping Z and W.
     * @param vec4 Source 4D vector.
     * @return 2D vector (vec4.X, vec4.Y).
     */
    static Vector2D ToVector2D(const Vector4D& vec4) {
        return Vector2D(vec4.X, vec4.Y);
    }

    /**
     * @brief Promote a Vector3D to a Vector4D by appending a W component.
     * @param vec3 Source 3D vector.
     * @param w Value for the W component (default 1, denoting a point).
     * @return 4D vector (vec3.X, vec3.Y, vec3.Z, w).
     */
    static Vector4D ToVector4D(const Vector3D& vec3, float w = 1.0f) {
        return Vector4D(vec3.X, vec3.Y, vec3.Z, w);
    }

    /**
     * @brief Truncate a Vector4D to a Vector3D by dropping the W component.
     * @param vec4 Source 4D vector.
     * @return 3D vector (vec4.X, vec4.Y, vec4.Z).
     */
    static Vector3D ToVector3D(const Vector4D& vec4) {
        return Vector3D(vec4.X, vec4.Y, vec4.Z);
    }
};
#endif