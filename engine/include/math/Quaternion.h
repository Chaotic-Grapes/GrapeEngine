/* Start Header *****************************************************************/
/*!
\file   Quaternion.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   12th March 2026
\brief
Quaternion math type and operations. Declares construction, length and normalization,
conjugate and inverse, Hamilton product, axis-angle and Euler angle construction,
spherical linear interpolation (Slerp), vector rotation and comparison operators.
Quaternions represent 3D rotations as (Xi + Yj + Zk + W) where W is the scalar part;
a unit quaternion encodes a rotation of 2*acos(W) radians around the axis (X, Y, Z).
*/
/* End Header *******************************************************************/

#ifndef QUATERNION_H
#define QUATERNION_H
#include "math/Vector3D.h"
#include "Export.h"

class GRAPEENGINE_API Quaternion {
public:
    float X, Y, Z, W;

    // ============================================================================
    // Constructors
    // ============================================================================

    Quaternion();
    Quaternion(float x, float y, float z, float w);

    // ============================================================================
    // Static constructors
    // ============================================================================

    static Quaternion Identity();

    // ============================================================================
    // Length and normalization
    // ============================================================================

    float Length() const;
    float SquareLength() const;
    void Normalize();
    Quaternion Normalized() const;

    // ============================================================================
    // Conjugate and inverse
    // ============================================================================

    Quaternion Conjugate() const;
    Quaternion Inverse() const;

    // ============================================================================
    // Multiplication
    // ============================================================================

    static Quaternion Multiply(const Quaternion& a, const Quaternion& b);

    // ============================================================================
    // Factory methods
    // ============================================================================

    static Quaternion FromAxisAngle(const Vector3D& axis, float angleRadians);
    static Quaternion FromEulerRad(float pitchX, float yawY, float rollZ);

    // ============================================================================
    // Interpolation
    // ============================================================================

    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);

    // ============================================================================
    // Vector rotation
    // ============================================================================

    Vector3D Rotate(const Vector3D& v) const;

    // ============================================================================
    // Comparison operators
    // ============================================================================

    bool operator==(const Quaternion& other) const;
    bool operator!=(const Quaternion& other) const;
};

// ============================================================================
// Non-member operator
// ============================================================================

GRAPEENGINE_API Quaternion operator*(const Quaternion& a, const Quaternion& b);

#endif // QUATERNION_H