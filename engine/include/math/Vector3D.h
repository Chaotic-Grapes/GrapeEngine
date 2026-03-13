/* Start Header *****************************************************************/
/*!
\file   Vector3D.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
3D vector math type and operations. Declares construction, arithmetic operators,
length and normalization, dot and cross products, distance queries,
interpolation and clamping utilities, and static direction constants.
*/
/* End Header *******************************************************************/

#ifndef VECTOR3D_H
#define VECTOR3D_H

#include "Export.h"

class GRAPEENGINE_API Vector3D {
public:
    float X, Y, Z;

    // ============================================================================
    // Constructors
    // ============================================================================

    Vector3D();
    Vector3D(float x, float y, float z);
    Vector3D(const Vector3D& other) = default;
    Vector3D& operator=(const Vector3D& other) = default;

    // ============================================================================
    // Assignment operators (member functions)
    // ============================================================================

    Vector3D& operator+=(const Vector3D& other);
    Vector3D& operator-=(const Vector3D& other);
    Vector3D& operator*=(float scalar);
    Vector3D& operator/=(float scalar);

    // ============================================================================
    // Unary operator
    // ============================================================================

    Vector3D operator-() const;

    // ============================================================================
    // Member functions
    // ============================================================================

    float Length() const;
    float SquareLength() const;
    Vector3D Normalized() const;  // Returns a unit-length copy without modifying this vector
    void Normalize();             // Normalizes this vector in place
    float Dot(const Vector3D& other) const;

    // ============================================================================
    // Static functions
    // ============================================================================

    static float Dot(const Vector3D& a, const Vector3D& b);
    static Vector3D Cross(const Vector3D& a, const Vector3D& b);
    static float Distance(const Vector3D& a, const Vector3D& b);
    static float SquareDistance(const Vector3D& a, const Vector3D& b);
    static Vector3D Lerp(const Vector3D& a, const Vector3D& b, float t);
    static Vector3D ClampVector(const Vector3D& vector, const Vector3D& min, const Vector3D& max);
    static float ClampValue(float component, float min, float max);

    // ============================================================================
    // Static members
    // ============================================================================

    static const Vector3D Zero;
    static const Vector3D Forward;
    static const Vector3D Up;
    static const Vector3D Right;
    static const Vector3D Backward;
    static const Vector3D Down;
    static const Vector3D Left;
};

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

GRAPEENGINE_API Vector3D operator+(const Vector3D& a, const Vector3D& b);
GRAPEENGINE_API Vector3D operator-(const Vector3D& a, const Vector3D& b);
GRAPEENGINE_API Vector3D operator*(const Vector3D& vector, float scalar);
GRAPEENGINE_API Vector3D operator*(float scalar, const Vector3D& vector);
GRAPEENGINE_API Vector3D operator/(const Vector3D& vector, float scalar);
GRAPEENGINE_API bool operator==(const Vector3D& a, const Vector3D& b);
GRAPEENGINE_API bool operator!=(const Vector3D& a, const Vector3D& b);

#endif // VECTOR3D_H