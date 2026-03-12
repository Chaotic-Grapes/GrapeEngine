/* Start Header *****************************************************************/
/*!
\file   Vector4D.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
4D vector math type and operations. Declares construction, arithmetic operators,
length and normalization, dot product, distance queries, interpolation,
clamping utilities, homogenization, point/vector W-flag helpers and XYZ extraction.
Primarily used to represent homogeneous coordinates for 3D transform pipelines
where W = 1 denotes a point and W = 0 denotes a direction vector.
*/
/* End Header *******************************************************************/

#ifndef VECTOR4D_H
#define VECTOR4D_H
#include "math/Vector3D.h"
#include "Export.h"

class GRAPEENGINE_API Vector4D {
public:
    float X, Y, Z, W;

    // ============================================================================
    // Constructors
    // ============================================================================

    Vector4D();
    Vector4D(float x, float y, float z, float w);
    Vector4D(const Vector4D& other) = default;
    Vector4D& operator=(const Vector4D& other) = default;

    // ============================================================================
    // Assignment operators (member functions)
    // ============================================================================

    Vector4D& operator+=(const Vector4D& other);
    Vector4D& operator-=(const Vector4D& other);
    Vector4D& operator*=(float scalar);
    Vector4D& operator/=(float scalar);

    // ============================================================================
    // Unary operator
    // ============================================================================

    Vector4D operator-() const;

    // ============================================================================
    // Member functions
    // ============================================================================

    float Length() const;
    float SquareLength() const;
    Vector4D Normalized() const;  // Returns a unit-length copy without modifying this vector
    void Normalize();             // Normalizes this vector in place
    float Dot(const Vector4D& other) const;

    // ============================================================================
    // Static functions
    // ============================================================================

    static float Dot(const Vector4D& a, const Vector4D& b);
    static float Distance(const Vector4D& a, const Vector4D& b);
    static float SquareDistance(const Vector4D& a, const Vector4D& b);
    static Vector4D Lerp(const Vector4D& a, const Vector4D& b, float t);
    static Vector4D Homogenize(const Vector4D& vector);
    static Vector4D ClampVector(const Vector4D& vector, const Vector4D& min, const Vector4D& max);
    static float ClampValue(float component, float min, float max);

    // ============================================================================
    // W-flag helpers
    // ============================================================================

    bool IsPoint() const;   // W == 1; translation affects this in matrix multiplication
    bool IsVector() const;  // W == 0; translation does not affect this in matrix multiplication
    Vector3D XYZ() const;   // Drop W and return just the spatial components

    // ============================================================================
    // Static members
    // ============================================================================

    static const Vector4D Zero;
};

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

Vector4D operator+(const Vector4D& a, const Vector4D& b);
Vector4D operator-(const Vector4D& a, const Vector4D& b);
Vector4D operator*(const Vector4D& vector, float scalar);
Vector4D operator*(float scalar, const Vector4D& vector);
Vector4D operator/(const Vector4D& vector, float scalar);
bool operator==(const Vector4D& a, const Vector4D& b);
bool operator!=(const Vector4D& a, const Vector4D& b);

#endif // VECTOR4D_H