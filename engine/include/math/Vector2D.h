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

    /**
     * @brief Construct a zero vector.
     */
    Vector2D();

    /**
     * @brief Construct a vector from components.
     * @param x X component.
     * @param y Y component.
     */
    Vector2D(float x, float y);
    Vector2D(const Vector2D& other) = default;
    Vector2D& operator=(const Vector2D& other) = default;

    // ============================================================================
    // Assignment operators (member functions)
    // ============================================================================

    /**
     * @brief Add another vector in place.
     * @param other Vector to add.
     * @return Reference to this vector.
     */
    Vector2D& operator+=(const Vector2D& other);

    /**
     * @brief Subtract another vector in place.
     * @param other Vector to subtract.
     * @return Reference to this vector.
     */
    Vector2D& operator-=(const Vector2D& other);

    /**
     * @brief Scale this vector in place.
     * @param scalar Scale factor.
     * @return Reference to this vector.
     */
    Vector2D& operator*=(float scalar);

    /**
     * @brief Divide this vector by a scalar in place.
     * @param scalar Divisor scalar.
     * @return Reference to this vector.
     */
    Vector2D& operator/=(float scalar);

    // ============================================================================
    // Unary operator
    // ============================================================================

    /**
     * @brief Return the negated vector.
     * @return Vector with both components negated.
     */
    Vector2D operator-() const;

    // ============================================================================
    // Member functions
    // ============================================================================

    /**
     * @brief Compute Euclidean length.
     * @return Vector magnitude.
     */
    float Length() const;

    /**
     * @brief Compute squared Euclidean length.
     * @return Squared magnitude.
     */
    float SquareLength() const;

    /**
     * @brief Return a normalized copy of this vector.
     * @return Unit-length copy when possible.
     */
    Vector2D Normalized() const;

    /**
     * @brief Normalize this vector in place.
     */
    void Normalize();

    /**
     * @brief Compute dot product with another vector.
     * @param other Right-hand operand.
     * @return Dot product.
     */
    float Dot(const Vector2D& other) const;

    // ============================================================================
    // Static functions
    // ============================================================================

    /**
     * @brief Compute dot product of two vectors.
     * @param a Left vector.
     * @param b Right vector.
     * @return Dot product.
     */
    static float Dot(const Vector2D& a, const Vector2D& b);

    /**
     * @brief Compute 2D scalar cross product.
     * @param a Left vector.
     * @param b Right vector.
     * @return Scalar z-component of the 3D cross product.
     */
    static float Cross(const Vector2D& a, const Vector2D& b);

    /**
     * @brief Compute distance between two vectors.
     * @param a First point.
     * @param b Second point.
     * @return Euclidean distance.
     */
    static float Distance(const Vector2D& a, const Vector2D& b);

    /**
     * @brief Compute squared distance between two vectors.
     * @param a First point.
     * @param b Second point.
     * @return Squared Euclidean distance.
     */
    static float SquareDistance(const Vector2D& a, const Vector2D& b);

    /**
     * @brief Linearly interpolate between two vectors.
     * @param a Start vector.
     * @param b End vector.
     * @param t Interpolation factor in [0, 1].
     * @return Interpolated vector.
     */
    static Vector2D Lerp(const Vector2D& a, const Vector2D& b, float t);

    /**
     * @brief Clamp vector components between min and max vectors.
     * @param vector Source vector.
     * @param min Minimum component bounds.
     * @param max Maximum component bounds.
     * @return Clamped vector.
     */
    static Vector2D ClampVector(const Vector2D& vector, const Vector2D& min, const Vector2D& max);

    /**
     * @brief Clamp one scalar component.
     * @param component Source value.
     * @param min Minimum bound.
     * @param max Maximum bound.
     * @return Clamped value.
     */
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

/**
 * @brief Add two vectors.
 * @param a Left vector.
 * @param b Right vector.
 * @return Sum of both vectors.
 */
Vector2D operator+(const Vector2D& a, const Vector2D& b);

/**
 * @brief Subtract two vectors.
 * @param a Left vector.
 * @param b Right vector.
 * @return Difference of both vectors.
 */
Vector2D operator-(const Vector2D& a, const Vector2D& b);

/**
 * @brief Scale a vector by a scalar.
 * @param vector Source vector.
 * @param scalar Scale factor.
 * @return Scaled vector.
 */
Vector2D operator*(const Vector2D& vector, float scalar);

/**
 * @brief Scale a vector by a scalar.
 * @param scalar Scale factor.
 * @param vector Source vector.
 * @return Scaled vector.
 */
Vector2D operator*(float scalar, const Vector2D& vector);

/**
 * @brief Divide a vector by a scalar.
 * @param vector Source vector.
 * @param scalar Divisor scalar.
 * @return Scaled vector.
 */
Vector2D operator/(const Vector2D& vector, float scalar);

/**
 * @brief Compare vectors for exact component equality.
 * @param a Left vector.
 * @param b Right vector.
 * @return True when both components are equal.
 */
bool operator==(const Vector2D& a, const Vector2D& b);

/**
 * @brief Compare vectors for exact component inequality.
 * @param a Left vector.
 * @param b Right vector.
 * @return True when at least one component differs.
 */
bool operator!=(const Vector2D& a, const Vector2D& b);

#endif // VECTOR2D_H