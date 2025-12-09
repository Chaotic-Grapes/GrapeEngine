#ifndef MATHHELPER_H
#define MATHHELPER_H

#include <random>
#include "math/Vector2D.h"
#include "math/Vector3D.h"
#include "math/Vector4D.h"

class MathUtils {
public:
    // Randomize function template
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
            std::uniform_int_distribution<T> dist(minVal, maxVal);
            return dist(engine);
        }
        else if constexpr (std::is_floating_point_v<T>) {
            std::uniform_real_distribution<T> dist(minVal, maxVal);
            return dist(engine);
        }
		else return 0; // Unsupported type
    }

    template <typename T>
    static T Clamp(T value, T minVal, T maxVal) {
        return value < minVal 
            ? minVal 
            : (value > maxVal ? maxVal : value);
    }

    // Min/Max for any comparable type
    template <typename T>
    static T Min(T a, T b) {
        return a < b ? a : b;
    }

    template <typename T>
    static T Max(T a, T b) {
        return a > b ? a : b;
    }

    // Abs for integral and floating-point types
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

    static Vector3D ToVector3D(const Vector2D& vec2, float z = 0.0f) {
        return Vector3D(vec2.X, vec2.Y, z);
    }

    static Vector4D ToVector4D(const Vector2D& vec2, float z = 0.0f, float w = 1.0f) {
        return Vector4D(vec2.X, vec2.Y, z, w);
    }

    static Vector2D ToVector2D(const Vector3D& vec3) {
        return Vector2D(vec3.X, vec3.Y);
    }

    static Vector2D ToVector2D(const Vector4D& vec4) {
        return Vector2D(vec4.X, vec4.Y);
    }
    
    static Vector4D ToVector4D(const Vector3D& vec3, float w = 1.0f) {
        return Vector4D(vec3.X, vec3.Y, vec3.Z, w);
    }

    static Vector3D ToVector3D(const Vector4D& vec4) {
        return Vector3D(vec4.X, vec4.Y, vec4.Z);
    }
};
#endif