#pragma once

#include <glm/vec2.hpp>
#include <glm/geometric.hpp> // length, distance, normalize, dot
#include <glm/common.hpp>    // clamp

class Vector2D {
public:
    float x;
    float y;

    // Constructors
    Vector2D();
    Vector2D(float _x, float _y);

    // In-place arithmetic
    Vector2D& operator+=(const Vector2D& rhs);
    Vector2D& operator-=(const Vector2D& rhs);
    Vector2D& operator*=(float rhs);
    Vector2D& operator/=(float rhs); // caller must avoid rhs == 0

    // Unary minus
    Vector2D operator-() const;

    // Queries
    float Length() const;
    float SquareLength() const;
    float Distance(const Vector2D& other) const;
    float SquareDistance(const Vector2D& other) const;
    float Dot(const Vector2D& other) const;
    float CrossProductMag(const Vector2D& other) const; // 

    // Normalization
    void     Normalize();        // in-place; zero stays (0,0)
    Vector2D Normalized() const; // returns unit copy

    // Clamp utilities (GLM-backed)
    static Vector2D Clamp(const Vector2D& v,
        const Vector2D& lo,
        const Vector2D& hi);
    static float    ClampFloat(float x, float lo, float hi);
};

// Non-member operators (value returning)
Vector2D operator+(const Vector2D& lhs, const Vector2D& rhs);
Vector2D operator-(const Vector2D& lhs, const Vector2D& rhs);
Vector2D operator*(const Vector2D& lhs, float rhs);
Vector2D operator*(float lhs, const Vector2D& rhs);
Vector2D operator/(const Vector2D& lhs, float rhs);