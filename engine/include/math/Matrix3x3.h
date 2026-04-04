/* Start Header *****************************************************************/
/*!
\file   Matrix3x3.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
3x3 matrix math type and operations. Declares construction, compound assignment operators,
element-wise and matrix multiplication, static factory methods for common 2D transforms
(translation, scale, rotation), transpose, inverse and non-member binary operators
including Matrix3x3 * Vector2D for affine point transform.
Matrix is stored row-major: mRC where R is the row index and C is the column index.
*/
/* End Header *******************************************************************/

#ifndef MATRIX3X3_H
#define MATRIX3X3_H
#include <cmath>
#include "math/Vector2D.h"
#include "Export.h"

class GRAPEENGINE_API Matrix3x3 {
public:
    float m00, m01, m02;
    float m10, m11, m12;
    float m20, m21, m22;

    // ============================================================================
    // Constructors
    // ============================================================================

    Matrix3x3();
    Matrix3x3(const float* pArr);
    Matrix3x3(float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22);
    Matrix3x3(const Matrix3x3& other) = default;
    Matrix3x3& operator=(const Matrix3x3& other) = default;

    // ============================================================================
    // Assignment operators (member functions)
    // ============================================================================

    /**
     * @brief Multiply this matrix by another in place.
     * @param other Right-hand matrix.
     * @return Reference to this matrix.
     */
    Matrix3x3& operator*=(const Matrix3x3& other);

    /**
     * @brief Add another matrix element-wise in place.
     * @param other Matrix to add.
     * @return Reference to this matrix.
     */
    Matrix3x3& operator+=(const Matrix3x3& other);

    /**
     * @brief Subtract another matrix element-wise in place.
     * @param other Matrix to subtract.
     * @return Reference to this matrix.
     */
    Matrix3x3& operator-=(const Matrix3x3& other);

    /**
     * @brief Scale this matrix by a scalar in place.
     * @param scalar Scalar multiplier.
     * @return Reference to this matrix.
     */
    Matrix3x3& operator*=(float scalar);

    /**
     * @brief Divide this matrix by a scalar in place.
     * @param scalar Scalar divisor (must not be zero).
     * @return Reference to this matrix.
     */
    Matrix3x3& operator/=(float scalar);

    // ============================================================================
    // Unary operator
    // ============================================================================

    /**
     * @brief Return the negation of this matrix (all elements negated).
     * @return Negated matrix.
     */
    Matrix3x3 operator-() const;

    // ============================================================================
    // Matrix operations
    // ============================================================================

    /** @brief Return the 3x3 identity matrix. */
    static Matrix3x3 Identity();

    /**
     * @brief Build a 2D translation matrix for homogeneous coordinates.
     * @param x Translation along X.
     * @param y Translation along Y.
     * @return Translation matrix.
     */
    static Matrix3x3 Translation(float x, float y);

    /**
     * @brief Build a 2D scale matrix.
     * @param x Scale factor along X.
     * @param y Scale factor along Y.
     * @return Scale matrix.
     */
    static Matrix3x3 Scaling(float x, float y);

    /**
     * @brief Build a 2D rotation matrix from an angle in radians.
     * @param angle Rotation angle in radians (counter-clockwise).
     * @return Rotation matrix.
     */
    static Matrix3x3 RotationRad(float angle);

    /**
     * @brief Build a 2D rotation matrix from an angle in degrees.
     * @param angle Rotation angle in degrees (counter-clockwise).
     * @return Rotation matrix.
     */
    static Matrix3x3 RotationDeg(float angle);

    /**
     * @brief Return the transpose of this matrix (rows become columns).
     * @return Transposed matrix.
     */
    Matrix3x3 Transpose() const;

    /**
     * @brief Return the inverse of this matrix.
     * @param determinant Optional output for the determinant; nullptr to discard.
     * @return Inverse matrix; undefined if the determinant is zero.
     */
    Matrix3x3 Inverse(float* determinant = nullptr) const;
};

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

/**
 * @brief Add two matrices element-wise.
 * @param a Left-hand matrix.
 * @param b Right-hand matrix.
 * @return Element-wise sum.
 */
Matrix3x3 operator+(const Matrix3x3& a, const Matrix3x3& b);

/**
 * @brief Subtract two matrices element-wise.
 * @param a Left-hand matrix.
 * @param b Right-hand matrix.
 * @return Element-wise difference.
 */
Matrix3x3 operator-(const Matrix3x3& a, const Matrix3x3& b);

/**
 * @brief Scale a matrix by a scalar.
 * @param matrix Matrix to scale.
 * @param scalar Scalar multiplier.
 * @return Scaled matrix.
 */
Matrix3x3 operator*(const Matrix3x3& matrix, float scalar);

/**
 * @brief Scale a matrix by a scalar (scalar on left).
 * @param scalar Scalar multiplier.
 * @param matrix Matrix to scale.
 * @return Scaled matrix.
 */
Matrix3x3 operator*(float scalar, const Matrix3x3& matrix);

/**
 * @brief Multiply two matrices.
 * @param a Left-hand matrix.
 * @param b Right-hand matrix.
 * @return Matrix product a * b.
 */
Matrix3x3 operator*(const Matrix3x3& a, const Matrix3x3& b);
/**
 * @brief Transform a 2D point by a homogeneous 3x3 matrix.
 * @param a Transform matrix.
 * @param b 2D point (W is implicitly 1).
 * @return Transformed 2D point.
 */
Vector2D operator*(const Matrix3x3& a, const Vector2D& b);
/**
 * @brief Divide a matrix by a scalar.
 * @param matrix Matrix to divide.
 * @param scalar Scalar divisor (must not be zero).
 * @return Divided matrix.
 */
Matrix3x3 operator/(const Matrix3x3& matrix, float scalar);

/**
 * @brief Return true if two matrices are element-wise equal.
 * @param a First matrix.
 * @param b Second matrix.
 * @return True when all elements match.
 */
bool operator==(const Matrix3x3& a, const Matrix3x3& b);

/**
 * @brief Return true if two matrices differ in at least one element.
 * @param a First matrix.
 * @param b Second matrix.
 * @return True when any element differs.
 */
bool operator!=(const Matrix3x3& a, const Matrix3x3& b);

// ============================================================================
// Constants
// ============================================================================

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

#endif // MATRIX3X3_H