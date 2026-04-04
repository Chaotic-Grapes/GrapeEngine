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

    /**
     * @brief Add another vector in place.
     * @param other Vector to add.
     * @return Reference to this vector.
     */
    Vector3D& operator+=(const Vector3D& other);

    /**
     * @brief Subtract another vector in place.
     * @param other Vector to subtract.
     * @return Reference to this vector.
     */
    Vector3D& operator-=(const Vector3D& other);

    /**
     * @brief Scale this vector in place.
     * @param scalar Scalar multiplier.
     * @return Reference to this vector.
     */
    Vector3D& operator*=(float scalar);

    /**
     * @brief Divide this vector in place.
     * @param scalar Scalar divisor (must not be zero).
     * @return Reference to this vector.
     */
    Vector3D& operator/=(float scalar);

    // ============================================================================
    // Unary operator
    // ============================================================================

    /**
     * @brief Return the negation of this vector.
     * @return Vector with all components negated.
     */
    Vector3D operator-() const;

    // ============================================================================
    // Member functions
    // ============================================================================

    /**
     * @brief Return the Euclidean length of this vector.
     * @return Square root of the sum of squared components.
     */
    float Length() const;

    /**
     * @brief Return the squared Euclidean length of this vector.
     * @return Sum of squared components (avoids a sqrt).
     */
    float SquareLength() const;

    /**
     * @brief Return a unit-length copy of this vector without modifying it.
     * @return Normalized copy, or a zero vector if length is zero.
     */
    Vector3D Normalized() const;

    /** @brief Normalize this vector in place; becomes a zero vector if length is zero. */
    void Normalize();

    /**
     * @brief Compute the dot product of this vector with another.
     * @param other The other vector.
     * @return Scalar dot product.
     */
    float Dot(const Vector3D& other) const;

    // ============================================================================
    // Static functions
    // ============================================================================

    /**
     * @brief Compute the dot product of two vectors.
     * @param a First vector.
     * @param b Second vector.
     * @return Scalar dot product.
     */
    static float Dot(const Vector3D& a, const Vector3D& b);

    /**
     * @brief Compute the cross product of two vectors.
     * @param a First vector.
     * @param b Second vector.
     * @return Vector perpendicular to both a and b.
     */
    static Vector3D Cross(const Vector3D& a, const Vector3D& b);

    /**
     * @brief Compute the Euclidean distance between two points.
     * @param a First point.
     * @param b Second point.
     * @return Distance between a and b.
     */
    static float Distance(const Vector3D& a, const Vector3D& b);

    /**
     * @brief Compute the squared Euclidean distance between two points.
     * @param a First point.
     * @param b Second point.
     * @return Squared distance between a and b (avoids a sqrt).
     */
    static float SquareDistance(const Vector3D& a, const Vector3D& b);

    /**
     * @brief Linearly interpolate between two vectors.
     * @param a Start vector (t = 0).
     * @param b End vector (t = 1).
     * @param t Interpolation factor in [0, 1].
     * @return Interpolated vector.
     */
    static Vector3D Lerp(const Vector3D& a, const Vector3D& b, float t);

    /**
     * @brief Clamp each component of a vector between per-component min and max.
     * @param vector Vector to clamp.
     * @param min Per-component minimum values.
     * @param max Per-component maximum values.
     * @return Clamped vector.
     */
    static Vector3D ClampVector(const Vector3D& vector, const Vector3D& min, const Vector3D& max);

    /**
     * @brief Clamp a scalar value between min and max.
     * @param component Value to clamp.
     * @param min Minimum bound.
     * @param max Maximum bound.
     * @return Clamped value.
     */
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

/**
 * @brief Add two vectors component-wise.
 * @param a Left-hand vector.
 * @param b Right-hand vector.
 * @return Component-wise sum.
 */
GRAPEENGINE_API Vector3D operator+(const Vector3D& a, const Vector3D& b);

/**
 * @brief Subtract two vectors component-wise.
 * @param a Left-hand vector.
 * @param b Right-hand vector.
 * @return Component-wise difference.
 */
GRAPEENGINE_API Vector3D operator-(const Vector3D& a, const Vector3D& b);

/**
 * @brief Scale a vector by a scalar.
 * @param vector Vector to scale.
 * @param scalar Scalar multiplier.
 * @return Scaled vector.
 */
GRAPEENGINE_API Vector3D operator*(const Vector3D& vector, float scalar);

/**
 * @brief Scale a vector by a scalar (scalar on left).
 * @param scalar Scalar multiplier.
 * @param vector Vector to scale.
 * @return Scaled vector.
 */
GRAPEENGINE_API Vector3D operator*(float scalar, const Vector3D& vector);

/**
 * @brief Divide a vector by a scalar.
 * @param vector Vector to divide.
 * @param scalar Scalar divisor (must not be zero).
 * @return Divided vector.
 */
GRAPEENGINE_API Vector3D operator/(const Vector3D& vector, float scalar);

/**
 * @brief Return true if two vectors are component-wise equal.
 * @param a First vector.
 * @param b Second vector.
 * @return True when all components match.
 */
GRAPEENGINE_API bool operator==(const Vector3D& a, const Vector3D& b);

/**
 * @brief Return true if two vectors differ in at least one component.
 * @param a First vector.
 * @param b Second vector.
 * @return True when any component differs.
 */
GRAPEENGINE_API bool operator!=(const Vector3D& a, const Vector3D& b);

#endif // VECTOR3D_H