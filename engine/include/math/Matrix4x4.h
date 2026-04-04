/* Start Header *****************************************************************/
/*!
\file   Matrix4x4.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
4x4 matrix math type and operations. Declares construction, compound assignment operators,
element-wise and matrix multiplication, static factory methods for 3D transforms
(translation, scale, axis rotations), orthographic and perspective projection,
LookAt view matrix, transpose, inverse and non-member binary operators
including Matrix4x4 * Vector3D and Matrix4x4 * Vector4D.
Matrix is stored row-major: mRC where R is the row index and C is the column index.
*/
/* End Header *******************************************************************/

#ifndef MATRIX4X4_H
#define MATRIX4X4_H
#include <cmath>
#include "math/Vector3D.h"
#include "math/Vector4D.h"
#include "Export.h"

class GRAPEENGINE_API Matrix4x4 {
public:
    float m00, m01, m02, m03;
    float m10, m11, m12, m13;
    float m20, m21, m22, m23;
    float m30, m31, m32, m33;

    // ============================================================================
    // Constructors
    // ============================================================================

    Matrix4x4();
    Matrix4x4(const float* pArr);
    Matrix4x4(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33);
    Matrix4x4(const Matrix4x4& other) = default;
    Matrix4x4& operator=(const Matrix4x4& other) = default;

    // ============================================================================
    // Assignment operators (member functions)
    // ============================================================================

    /**
     * @brief Multiply this matrix by another in place.
     * @param other Right-hand matrix.
     * @return Reference to this matrix.
     */
    Matrix4x4& operator*=(const Matrix4x4& other);

    /**
     * @brief Add another matrix element-wise in place.
     * @param other Matrix to add.
     * @return Reference to this matrix.
     */
    Matrix4x4& operator+=(const Matrix4x4& other);

    /**
     * @brief Subtract another matrix element-wise in place.
     * @param other Matrix to subtract.
     * @return Reference to this matrix.
     */
    Matrix4x4& operator-=(const Matrix4x4& other);

    /**
     * @brief Scale this matrix by a scalar in place.
     * @param scalar Scalar multiplier.
     * @return Reference to this matrix.
     */
    Matrix4x4& operator*=(float scalar);

    /**
     * @brief Divide this matrix by a scalar in place.
     * @param scalar Scalar divisor (must not be zero).
     * @return Reference to this matrix.
     */
    Matrix4x4& operator/=(float scalar);

    // ============================================================================
    // Unary operator
    // ============================================================================

    /**
     * @brief Return the negation of this matrix (all elements negated).
     * @return Negated matrix.
     */
    Matrix4x4 operator-() const;

    // ============================================================================
    // Matrix operations
    // ============================================================================

    /** @brief Return the 4x4 identity matrix. */
    static Matrix4x4 Identity();

    /**
     * @brief Build a 3D translation matrix.
     * @param x Translation along X.
     * @param y Translation along Y.
     * @param z Translation along Z.
     * @return Translation matrix.
     */
    static Matrix4x4 Translation(float x, float y, float z);

    /**
     * @brief Build a 3D scale matrix.
     * @param x Scale factor along X.
     * @param y Scale factor along Y.
     * @param z Scale factor along Z.
     * @return Scale matrix.
     */
    static Matrix4x4 Scaling(float x, float y, float z);

    /**
     * @brief Build a rotation matrix around the X axis.
     * @param angle Rotation angle in radians.
     * @return Rotation matrix.
     */
    static Matrix4x4 RotationX(float angle);

    /**
     * @brief Build a rotation matrix around the Y axis.
     * @param angle Rotation angle in radians.
     * @return Rotation matrix.
     */
    static Matrix4x4 RotationY(float angle);

    /**
     * @brief Build a rotation matrix around the Z axis.
     * @param angle Rotation angle in radians.
     * @return Rotation matrix.
     */
    static Matrix4x4 RotationZ(float angle);

    // ============================================================================
    // Projection matrices
    // ============================================================================

    /**
     * @brief Build an orthographic projection matrix.
     * @param left Left clipping plane.
     * @param right Right clipping plane.
     * @param bottom Bottom clipping plane.
     * @param top Top clipping plane.
     * @param near Near clipping distance.
     * @param far Far clipping distance.
     * @return Orthographic projection matrix.
     */
    static Matrix4x4 Orthographic(float left, float right, float bottom, float top, float near, float far);

    /**
     * @brief Build a perspective projection matrix.
     * @param fov Vertical field-of-view angle in radians.
     * @param aspect Viewport aspect ratio (width / height).
     * @param near Near clipping distance.
     * @param far Far clipping distance.
     * @return Perspective projection matrix.
     */
    static Matrix4x4 Perspective(float fov, float aspect, float near, float far);

    // ============================================================================
    // View matrix
    // ============================================================================

    /**
     * @brief Build a view (camera) matrix that looks from eye toward target.
     * @param eye Camera position in world space.
     * @param target Point the camera looks at.
     * @param up World up vector used to resolve camera roll.
     * @return View matrix that transforms world space to camera space.
     */
    static Matrix4x4 LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up);

    /**
     * @brief Return the transpose of this matrix (rows become columns).
     * @return Transposed matrix.
     */
    Matrix4x4 Transpose() const;

    /**
     * @brief Return the inverse of this matrix.
     * @param determinant Optional output for the determinant; nullptr to discard.
     * @return Inverse matrix; undefined if the determinant is zero.
     */
    Matrix4x4 Inverse(float* determinant = nullptr) const;
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
GRAPEENGINE_API Matrix4x4 operator+(const Matrix4x4& a, const Matrix4x4& b);

/**
 * @brief Subtract two matrices element-wise.
 * @param a Left-hand matrix.
 * @param b Right-hand matrix.
 * @return Element-wise difference.
 */
GRAPEENGINE_API Matrix4x4 operator-(const Matrix4x4& a, const Matrix4x4& b);

/**
 * @brief Scale a matrix by a scalar.
 * @param matrix Matrix to scale.
 * @param scalar Scalar multiplier.
 * @return Scaled matrix.
 */
GRAPEENGINE_API Matrix4x4 operator*(const Matrix4x4& matrix, float scalar);

/**
 * @brief Scale a matrix by a scalar (scalar on left).
 * @param scalar Scalar multiplier.
 * @param matrix Matrix to scale.
 * @return Scaled matrix.
 */
GRAPEENGINE_API Matrix4x4 operator*(float scalar, const Matrix4x4& matrix);

/**
 * @brief Multiply two matrices.
 * @param a Left-hand matrix.
 * @param b Right-hand matrix.
 * @return Matrix product a * b.
 */
GRAPEENGINE_API Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b);

// ============================================================================
// Vector transformations
// ============================================================================

/**
 * @brief Transform a 3D point by a 4x4 matrix (W is implicitly 1).
 * @param a Transform matrix.
 * @param b 3D point to transform.
 * @return Transformed 3D point.
 */
GRAPEENGINE_API Vector3D operator*(const Matrix4x4& a, const Vector3D& b);

/**
 * @brief Transform a homogeneous 4D vector by a 4x4 matrix.
 * @param a Transform matrix.
 * @param b 4D homogeneous vector.
 * @return Transformed 4D vector.
 */
GRAPEENGINE_API Vector4D operator*(const Matrix4x4& a, const Vector4D& b);

/**
 * @brief Divide a matrix by a scalar.
 * @param matrix Matrix to divide.
 * @param scalar Scalar divisor (must not be zero).
 * @return Divided matrix.
 */
GRAPEENGINE_API Matrix4x4 operator/(const Matrix4x4& matrix, float scalar);

/**
 * @brief Return true if two matrices are element-wise equal.
 * @param a First matrix.
 * @param b Second matrix.
 * @return True when all elements match.
 */
GRAPEENGINE_API bool operator==(const Matrix4x4& a, const Matrix4x4& b);

/**
 * @brief Return true if two matrices differ in at least one element.
 * @param a First matrix.
 * @param b Second matrix.
 * @return True when any element differs.
 */
bool operator!=(const Matrix4x4& a, const Matrix4x4& b);

// ============================================================================
// Constants
// ============================================================================

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

#endif // MATRIX4X4_H