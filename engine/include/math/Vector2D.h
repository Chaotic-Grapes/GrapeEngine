/* Start Header *****************************************************************/
/*!
\file   Vector2D.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
2D vector math type and operations. Declares construction, arithmetic operators,
length and normalization, dot and cross products, distance queries,
interpolation and clamping utilities, and static direction constants.
*/
/* End Header *******************************************************************/

#ifndef VECTOR2D_H
#define VECTOR2D_H

#include "Export.h"

class GRAPEENGINE_API Vector2D {
public:
    float X, Y;

    // ============================================================================
    // Constructors
    // ============================================================================

    Vector2D();
    Vector2D(float x, float y);
    Vector2D(const Vector2D& other) = default;
    Vector2D& operator=(const Vector2D& other) = default;

    // ============================================================================
    // Assignment operators (member functions)
    // ============================================================================

    Vector2D& operator+=(const Vector2D& other);
    Vector2D& operator-=(const Vector2D& other);
    Vector2D& operator*=(float scalar);
    Vector2D& operator/=(float scalar);

    // ============================================================================
    // Unary operator
    // ============================================================================

    Vector2D operator-() const;

    // ============================================================================
    // Member functions
    // ============================================================================

    float Length() const;
    float SquareLength() const;
    Vector2D Normalized() const;  // Returns a unit-length copy without modifying this vector
    void Normalize();             // Normalizes this vector in place
    float Dot(const Vector2D& other) const;

    // ============================================================================
    // Static functions
    // ============================================================================

    static float Dot(const Vector2D& a, const Vector2D& b);
    static float Cross(const Vector2D& a, const Vector2D& b);
    static float Distance(const Vector2D& a, const Vector2D& b);
    static float SquareDistance(const Vector2D& a, const Vector2D& b);
    static Vector2D Lerp(const Vector2D& a, const Vector2D& b, float t);
    static Vector2D ClampVector(const Vector2D& vector, const Vector2D& min, const Vector2D& max);
    static float ClampValue(float component, float min, float max);

    // ============================================================================
    // Static members
    // ============================================================================

    static const Vector2D Zero;
    static const Vector2D Up;
    static const Vector2D Right;
    static const Vector2D Down;
    static const Vector2D Left;
};

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

Vector2D operator+(const Vector2D& a, const Vector2D& b);
Vector2D operator-(const Vector2D& a, const Vector2D& b);
Vector2D operator*(const Vector2D& vector, float scalar);
Vector2D operator*(float scalar, const Vector2D& vector);
Vector2D operator/(const Vector2D& vector, float scalar);
bool operator==(const Vector2D& a, const Vector2D& b);
bool operator!=(const Vector2D& a, const Vector2D& b);

#endif // VECTOR2D_H