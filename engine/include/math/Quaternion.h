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

    /** @brief Return the identity quaternion (X=0, Y=0, Z=0, W=1; represents no rotation). */
    static Quaternion Identity();

    // ============================================================================
    // Length and normalization
    // ============================================================================

    /**
     * @brief Return the Euclidean length of the quaternion.
     * @return Square root of the sum of squared components.
     */
    float Length() const;

    /**
     * @brief Return the squared Euclidean length of the quaternion.
     * @return Sum of squared components (avoids a sqrt).
     */
    float SquareLength() const;

    /** @brief Normalize this quaternion in place; becomes identity if length is zero. */
    void Normalize();

    /**
     * @brief Return a unit-length copy of this quaternion without modifying it.
     * @return Normalized quaternion, or identity if length is zero.
     */
    Quaternion Normalized() const;

    // ============================================================================
    // Conjugate and inverse
    // ============================================================================

    /**
     * @brief Return the conjugate of this quaternion (negate the vector part).
     * @return Conjugate quaternion (-X, -Y, -Z, W).
     */
    Quaternion Conjugate() const;

    /**
     * @brief Return the multiplicative inverse of this quaternion.
     * @return Inverse quaternion; equivalent to conjugate for unit quaternions.
     */
    Quaternion Inverse() const;

    // ============================================================================
    // Multiplication
    // ============================================================================

    /**
     * @brief Compute the Hamilton product of two quaternions.
     * @param a Left quaternion.
     * @param b Right quaternion.
     * @return Combined rotation a followed by b.
     */
    static Quaternion Multiply(const Quaternion& a, const Quaternion& b);

    // ============================================================================
    // Factory methods
    // ============================================================================

    /**
     * @brief Build a quaternion from an axis and an angle.
     * @param axis Unit rotation axis (will be normalized internally).
     * @param angleRadians Rotation angle in radians.
     * @return Unit quaternion representing the rotation.
     */
    static Quaternion FromAxisAngle(const Vector3D& axis, float angleRadians);

    /**
     * @brief Build a quaternion from Euler angles (intrinsic XYZ order).
     * @param pitchX Rotation around X axis in radians.
     * @param yawY Rotation around Y axis in radians.
     * @param rollZ Rotation around Z axis in radians.
     * @return Unit quaternion representing the combined rotation.
     */
    static Quaternion FromEulerRad(float pitchX, float yawY, float rollZ);

    // ============================================================================
    // Interpolation
    // ============================================================================

    /**
     * @brief Spherically interpolate between two quaternions.
     * @param a Start rotation (t = 0).
     * @param b End rotation (t = 1).
     * @param t Interpolation factor in [0, 1].
     * @return Interpolated unit quaternion.
     */
    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);

    // ============================================================================
    // Vector rotation
    // ============================================================================

    /**
     * @brief Rotate a vector by this quaternion.
     * @param v Vector to rotate.
     * @return Rotated vector.
     */
    Vector3D Rotate(const Vector3D& v) const;

    // ============================================================================
    // Comparison operators
    // ============================================================================

    /**
     * @brief Return true if two quaternions are component-wise equal.
     * @param other Quaternion to compare against.
     * @return True when all four components match.
     */
    bool operator==(const Quaternion& other) const;

    /**
     * @brief Return true if two quaternions differ in at least one component.
     * @param other Quaternion to compare against.
     * @return True when at least one component differs.
     */
    bool operator!=(const Quaternion& other) const;
};

// ============================================================================
// Non-member operator
// ============================================================================

/**
 * @brief Multiply two quaternions (Hamilton product, same as Quaternion::Multiply).
 * @param a Left quaternion.
 * @param b Right quaternion.
 * @return Combined rotation a followed by b.
 */
GRAPEENGINE_API Quaternion operator*(const Quaternion& a, const Quaternion& b);

#endif // QUATERNION_H