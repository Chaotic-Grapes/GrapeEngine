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

    /**
     * @brief Add another vector in place.
     * @param other Vector to add.
     * @return Reference to this vector.
     */
    Vector4D& operator+=(const Vector4D& other);

    /**
     * @brief Subtract another vector in place.
     * @param other Vector to subtract.
     * @return Reference to this vector.
     */
    Vector4D& operator-=(const Vector4D& other);

    /**
     * @brief Scale this vector in place.
     * @param scalar Scalar multiplier.
     * @return Reference to this vector.
     */
    Vector4D& operator*=(float scalar);

    /**
     * @brief Divide this vector in place.
     * @param scalar Scalar divisor (must not be zero).
     * @return Reference to this vector.
     */
    Vector4D& operator/=(float scalar);

    // ============================================================================
    // Unary operator
    // ============================================================================

    /**
     * @brief Return the negation of this vector.
     * @return Vector with all components negated.
     */
    Vector4D operator-() const;

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
    Vector4D Normalized() const;

    /** @brief Normalize this vector in place; becomes a zero vector if length is zero. */
    void Normalize();

    /**
     * @brief Compute the dot product of this vector with another.
     * @param other The other vector.
     * @return Scalar dot product.
     */
    float Dot(const Vector4D& other) const;

    // ============================================================================
    // Static functions
    // ============================================================================

    /**
     * @brief Compute the dot product of two vectors.
     * @param a First vector.
     * @param b Second vector.
     * @return Scalar dot product.
     */
    static float Dot(const Vector4D& a, const Vector4D& b);

    /**
     * @brief Compute the Euclidean distance between two points.
     * @param a First point.
     * @param b Second point.
     * @return Distance between a and b.
     */
    static float Distance(const Vector4D& a, const Vector4D& b);

    /**
     * @brief Compute the squared Euclidean distance between two points.
     * @param a First point.
     * @param b Second point.
     * @return Squared distance (avoids a sqrt).
     */
    static float SquareDistance(const Vector4D& a, const Vector4D& b);

    /**
     * @brief Linearly interpolate between two vectors.
     * @param a Start vector (t = 0).
     * @param b End vector (t = 1).
     * @param t Interpolation factor in [0, 1].
     * @return Interpolated vector.
     */
    static Vector4D Lerp(const Vector4D& a, const Vector4D& b, float t);

    /**
     * @brief Divide X, Y, Z by W to produce Cartesian coordinates.
     * @param vector Homogeneous vector to normalize (W must not be zero).
     * @return Vector with W = 1 and XYZ divided by the original W.
     */
    static Vector4D Homogenize(const Vector4D& vector);

    /**
     * @brief Clamp each component of a vector between per-component min and max.
     * @param vector Vector to clamp.
     * @param min Per-component minimum values.
     * @param max Per-component maximum values.
     * @return Clamped vector.
     */
    static Vector4D ClampVector(const Vector4D& vector, const Vector4D& min, const Vector4D& max);

    /**
     * @brief Clamp a scalar value between min and max.
     * @param component Value to clamp.
     * @param min Minimum bound.
     * @param max Maximum bound.
     * @return Clamped value.
     */
    static float ClampValue(float component, float min, float max);

    // ============================================================================
    // W-flag helpers
    // ============================================================================

    /** @brief Return true when W == 1 (point; translation applies in matrix multiplication). */
    bool IsPoint() const;

    /** @brief Return true when W == 0 (direction; translation does not apply in matrix multiplication). */
    bool IsVector() const;

    /** @brief Drop W and return just the XYZ spatial components as a Vector3D. */
    Vector3D XYZ() const;

    // ============================================================================
    // Static members
    // ============================================================================

    static const Vector4D Zero;
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
Vector4D operator+(const Vector4D& a, const Vector4D& b);

/**
 * @brief Subtract two vectors component-wise.
 * @param a Left-hand vector.
 * @param b Right-hand vector.
 * @return Component-wise difference.
 */
Vector4D operator-(const Vector4D& a, const Vector4D& b);

/**
 * @brief Scale a vector by a scalar.
 * @param vector Vector to scale.
 * @param scalar Scalar multiplier.
 * @return Scaled vector.
 */
Vector4D operator*(const Vector4D& vector, float scalar);

/**
 * @brief Scale a vector by a scalar (scalar on left).
 * @param scalar Scalar multiplier.
 * @param vector Vector to scale.
 * @return Scaled vector.
 */
Vector4D operator*(float scalar, const Vector4D& vector);

/**
 * @brief Divide a vector by a scalar.
 * @param vector Vector to divide.
 * @param scalar Scalar divisor (must not be zero).
 * @return Divided vector.
 */
Vector4D operator/(const Vector4D& vector, float scalar);

/**
 * @brief Return true if two vectors are component-wise equal.
 * @param a First vector.
 * @param b Second vector.
 * @return True when all components match.
 */
bool operator==(const Vector4D& a, const Vector4D& b);

/**
 * @brief Return true if two vectors differ in at least one component.
 * @param a First vector.
 * @param b Second vector.
 * @return True when any component differs.
 */
bool operator!=(const Vector4D& a, const Vector4D& b);

#endif // VECTOR4D_H