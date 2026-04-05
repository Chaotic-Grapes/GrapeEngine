/* Start Header *****************************************************************/
/*!
\file   Matrix3x3.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
3x3 matrix math implementation. Covers construction, compound assignment operators,
element-wise and matrix multiplication, static factory methods for common 2D transforms
(translation, scale, rotation), transpose, inverse via the adjugate/determinant method,
and non-member binary operators including Matrix3x3 * Vector2D for affine point transform.
Matrix is stored row-major: mRC where R is the row index and C is the column index.
*/
/* End Header *******************************************************************/

#include "math/Matrix3x3.h"

// ============================================================================
// Constructors
// ============================================================================

// Default-construct to the zero matrix; callers needing identity should use Matrix3x3::Identity()
Matrix3x3::Matrix3x3() :
	m00(0.0f), m01(0.0f), m02(0.0f),
	m10(0.0f), m11(0.0f), m12(0.0f),
	m20(0.0f), m21(0.0f), m22(0.0f) {
}

// Construct from a flat array in row-major order: [m00, m01, m02, m10, m11, m12, m20, m21, m22]
Matrix3x3::Matrix3x3(const float* pArr) :
	m00(pArr[0]), m01(pArr[1]), m02(pArr[2]),
	m10(pArr[3]), m11(pArr[4]), m12(pArr[5]),
	m20(pArr[6]), m21(pArr[7]), m22(pArr[8]) {
}

// Construct from nine explicit scalars listed in row-major order
Matrix3x3::Matrix3x3(float m00, float m01, float m02,
	float m10, float m11, float m12,
	float m20, float m21, float m22) :
	m00(m00), m01(m01), m02(m02),
	m10(m10), m11(m11), m12(m12),
	m20(m20), m21(m21), m22(m22) {
}

// ============================================================================
// Assignment operators (member functions)
// ============================================================================

/**
 * @brief Multiplies this matrix by another in-place (matrix multiplication).
 *        All output elements are computed into temporaries before any member is overwritten
 *        to avoid aliasing issues when this == &other.
 * @param other The right-hand matrix to multiply by.
 * @return Reference to this matrix after multiplication.
 */
Matrix3x3& Matrix3x3::operator*=(const Matrix3x3& other) {
	// Compute all nine output elements into temporaries before overwriting any member,
	// because each element of *this is reused across multiple dot products
	float temp00 = m00 * other.m00 + m01 * other.m10 + m02 * other.m20;
	float temp01 = m00 * other.m01 + m01 * other.m11 + m02 * other.m21;
	float temp02 = m00 * other.m02 + m01 * other.m12 + m02 * other.m22;

	float temp10 = m10 * other.m00 + m11 * other.m10 + m12 * other.m20;
	float temp11 = m10 * other.m01 + m11 * other.m11 + m12 * other.m21;
	float temp12 = m10 * other.m02 + m11 * other.m12 + m12 * other.m22;

	float temp20 = m20 * other.m00 + m21 * other.m10 + m22 * other.m20;
	float temp21 = m20 * other.m01 + m21 * other.m11 + m22 * other.m21;
	float temp22 = m20 * other.m02 + m21 * other.m12 + m22 * other.m22;

	// Safe to write back now that all reads are done
	m00 = temp00; m01 = temp01; m02 = temp02;
	m10 = temp10; m11 = temp11; m12 = temp12;
	m20 = temp20; m21 = temp21; m22 = temp22;

	return *this;
}

/**
 * @brief Adds another matrix to this matrix element-wise in-place.
 * @param other The matrix whose elements are added to this matrix.
 * @return Reference to this matrix after addition.
 */
Matrix3x3& Matrix3x3::operator+=(const Matrix3x3& other) {
	m00 += other.m00; m01 += other.m01; m02 += other.m02;
	m10 += other.m10; m11 += other.m11; m12 += other.m12;
	m20 += other.m20; m21 += other.m21; m22 += other.m22;
	return *this;
}

/**
 * @brief Subtracts another matrix from this matrix element-wise in-place.
 * @param other The matrix whose elements are subtracted from this matrix.
 * @return Reference to this matrix after subtraction.
 */
Matrix3x3& Matrix3x3::operator-=(const Matrix3x3& other) {
	m00 -= other.m00; m01 -= other.m01; m02 -= other.m02;
	m10 -= other.m10; m11 -= other.m11; m12 -= other.m12;
	m20 -= other.m20; m21 -= other.m21; m22 -= other.m22;
	return *this;
}

/**
 * @brief Scales all elements of this matrix uniformly by a scalar in-place.
 * @param scalar The value to multiply every element by.
 * @return Reference to this matrix after scaling.
 */
Matrix3x3& Matrix3x3::operator*=(float scalar) {
	m00 *= scalar; m01 *= scalar; m02 *= scalar;
	m10 *= scalar; m11 *= scalar; m12 *= scalar;
	m20 *= scalar; m21 *= scalar; m22 *= scalar;
	return *this;
}

/**
 * @brief Divides all elements of this matrix uniformly by a scalar in-place.
 *        Uses reciprocal multiplication to avoid nine separate divisions.
 * @param scalar The value to divide every element by.
 * @return Reference to this matrix after division.
 */
Matrix3x3& Matrix3x3::operator/=(float scalar) {
	float invScalar = 1.0f / scalar;
	m00 *= invScalar; m01 *= invScalar; m02 *= invScalar;
	m10 *= invScalar; m11 *= invScalar; m12 *= invScalar;
	m20 *= invScalar; m21 *= invScalar; m22 *= invScalar;
	return *this;
}

// ============================================================================
// Unary operator
// ============================================================================

/**
 * @brief Returns a copy of this matrix with all elements negated.
 *        Useful for reversing a signed offset matrix.
 * @return A new Matrix3x3 with every element sign-flipped.
 */
Matrix3x3 Matrix3x3::operator-() const {
	return Matrix3x3(
		-m00, -m01, -m02,
		-m10, -m11, -m12,
		-m20, -m21, -m22
	);
}

// ============================================================================
// Matrix operations
// ============================================================================

/**
 * @brief Returns the 3x3 multiplicative identity matrix.
 *        Transforms are built by multiplying against this matrix.
 * @return A Matrix3x3 with ones on the diagonal and zeros elsewhere.
 */
Matrix3x3 Matrix3x3::Identity() {
	return Matrix3x3(
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	);
}

/**
 * @brief Constructs a 2D affine translation matrix.
 *        The offset is encoded in the third column so that multiplying a homogeneous
 *        Vector2D (z = 1) adds the translation to the result.
 * @param x Translation along the X axis.
 * @param y Translation along the Y axis.
 * @return A Matrix3x3 representing the 2D translation.
 */
Matrix3x3 Matrix3x3::Translation(float x, float y) {
	return Matrix3x3(
		1.0f, 0.0f, x,
		0.0f, 1.0f, y,
		0.0f, 0.0f, 1.0f
	);
}

/**
 * @brief Constructs a 2D non-uniform scale matrix.
 *        Scales the X and Y axes independently.
 * @param x Scale factor along the X axis.
 * @param y Scale factor along the Y axis.
 * @return A Matrix3x3 representing the 2D scale.
 */
Matrix3x3 Matrix3x3::Scaling(float x, float y) {
	return Matrix3x3(
		x, 0.0f, 0.0f,
		0.0f, y, 0.0f,
		0.0f, 0.0f, 1.0f
	);
}

/**
 * @brief Constructs a 2D counter-clockwise rotation matrix from an angle in radians.
 *        The standard 2D rotation layout is applied:
 *        [ cos  -sin  0 ]
 *        [ sin   cos  0 ]
 *        [  0     0   1 ]
 * @param angle Rotation angle in radians.
 * @return A Matrix3x3 representing the 2D CCW rotation.
 */
Matrix3x3 Matrix3x3::RotationRad(float angle) {
	float cosA = cosf(angle);
	float sinA = sinf(angle);

	return Matrix3x3(
		cosA, -sinA, 0.0f,
		sinA, cosA, 0.0f,
		0.0f, 0.0f, 1.0f
	);
}

/**
 * @brief Constructs a 2D counter-clockwise rotation matrix from an angle in degrees.
 *        Converts degrees to radians then delegates to RotationRad.
 * @param angle Rotation angle in degrees.
 * @return A Matrix3x3 representing the 2D CCW rotation.
 */
Matrix3x3 Matrix3x3::RotationDeg(float angle) {
	float angleRad = angle * DEG_TO_RAD;
	return RotationRad(angleRad);
}

/**
 * @brief Returns the transpose of this matrix (rows and columns swapped).
 *        For pure rotation matrices, the transpose is equivalent to the inverse.
 * @return A new Matrix3x3 that is the transpose of this matrix.
 */
Matrix3x3 Matrix3x3::Transpose() const {
	return Matrix3x3(
		m00, m10, m20,
		m01, m11, m21,
		m02, m12, m22
	);
}

/**
 * @brief Computes the inverse of this matrix via the classical adjugate formula.
 *        A^-1 = adj(A) / det(A). If the determinant is zero the matrix is singular
 *        and the zero matrix is returned as a safe fallback.
 * @param determinant Optional pointer that receives the computed determinant; may be nullptr.
 * @return The inverse Matrix3x3, or the zero matrix if this matrix is singular.
 */
Matrix3x3 Matrix3x3::Inverse(float* determinant) const {
	// Cofactor expansion along the first row to get the scalar determinant;
	// each term is a 2x2 minor multiplied by its row-0 element
	float det =
		m00 * (m11 * m22 - m12 * m21) -
		m01 * (m10 * m22 - m12 * m20) +
		m02 * (m10 * m21 - m11 * m20);

	// Write out the determinant if the caller wants it (e.g. to detect singularity)
	if (determinant) *determinant = det;

	// Singular matrix has no inverse; return zero matrix as a safe fallback
	if (det == 0.0f) return Matrix3x3();

	float invDet = 1.0f / det;

	// Each element of the adjugate is the cofactor of the *transposed* position,
	// meaning element [r][c] uses the 2x2 minor opposite to original position [c][r],
	// with sign = (-1)^(r+c); multiplying by invDet gives the final inverse entries
	return Matrix3x3(
		// Row 0
		(m11 * m22 - m12 * m21) * invDet,   // cofactor C00
		(m02 * m21 - m01 * m22) * invDet,   // cofactor C10 (transposed from [1][0])
		(m01 * m12 - m02 * m11) * invDet,   // cofactor C20 (transposed from [2][0])

		// Row 1
		(m12 * m20 - m10 * m22) * invDet,   // cofactor C01 (transposed from [0][1])
		(m00 * m22 - m02 * m20) * invDet,   // cofactor C11
		(m02 * m10 - m00 * m12) * invDet,   // cofactor C21 (transposed from [2][1])

		// Row 2
		(m10 * m21 - m11 * m20) * invDet,   // cofactor C02 (transposed from [0][2])
		(m01 * m20 - m00 * m21) * invDet,   // cofactor C12 (transposed from [1][2])
		(m00 * m11 - m01 * m10) * invDet    // cofactor C22
	);
}

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

/**
 * @brief Adds two matrices element-wise.
 * @param a The left-hand matrix.
 * @param b The right-hand matrix.
 * @return A new Matrix3x3 containing the element-wise sum.
 */
Matrix3x3 operator+(const Matrix3x3& a, const Matrix3x3& b) {
	return Matrix3x3(
		a.m00 + b.m00, a.m01 + b.m01, a.m02 + b.m02,
		a.m10 + b.m10, a.m11 + b.m11, a.m12 + b.m12,
		a.m20 + b.m20, a.m21 + b.m21, a.m22 + b.m22
	);
}

/**
 * @brief Subtracts one matrix from another element-wise.
 * @param a The left-hand matrix.
 * @param b The right-hand matrix to subtract.
 * @return A new Matrix3x3 containing the element-wise difference.
 */
Matrix3x3 operator-(const Matrix3x3& a, const Matrix3x3& b) {
	return Matrix3x3(
		a.m00 - b.m00, a.m01 - b.m01, a.m02 - b.m02,
		a.m10 - b.m10, a.m11 - b.m11, a.m12 - b.m12,
		a.m20 - b.m20, a.m21 - b.m21, a.m22 - b.m22
	);
}

/**
 * @brief Multiplies every element of a matrix by a scalar.
 * @param matrix The matrix to scale.
 * @param scalar The scalar factor to apply to each element.
 * @return A new Matrix3x3 with each element uniformly scaled.
 */
Matrix3x3 operator*(const Matrix3x3& matrix, float scalar) {
	return Matrix3x3(
		matrix.m00 * scalar, matrix.m01 * scalar, matrix.m02 * scalar,
		matrix.m10 * scalar, matrix.m11 * scalar, matrix.m12 * scalar,
		matrix.m20 * scalar, matrix.m21 * scalar, matrix.m22 * scalar
	);
}

/**
 * @brief Multiplies every element of a matrix by a scalar (scalar on the left).
 *        Commutative overload so that scalar * matrix compiles equally.
 * @param scalar The scalar factor to apply to each element.
 * @param matrix The matrix to scale.
 * @return A new Matrix3x3 with each element uniformly scaled.
 */
Matrix3x3 operator*(float scalar, const Matrix3x3& matrix) { return matrix * scalar; }

/**
 * @brief Performs full 3x3 matrix multiplication.
 *        result[r][c] = dot(row r of a, column c of b).
 * @param a The left-hand matrix.
 * @param b The right-hand matrix.
 * @return A new Matrix3x3 that is the matrix product of a and b.
 */
Matrix3x3 operator*(const Matrix3x3& a, const Matrix3x3& b) {
	return Matrix3x3(
		// Row 0
		a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20,
		a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21,
		a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22,

		// Row 1
		a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20,
		a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21,
		a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22,

		// Row 2
		a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20,
		a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21,
		a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22
	);
}

/**
 * @brief Transforms a 2D point by a 3x3 affine matrix.
 *        The Vector2D is implicitly extended to (X, Y, 1) so the third-column
 *        translation (m02, m12) is fully applied. The W row is discarded
 *        since the return type is 2D.
 * @param a The 3x3 affine transform matrix.
 * @param b The 2D point to transform (treated as homogeneous with z = 1).
 * @return The transformed Vector2D.
 */
Vector2D operator*(const Matrix3x3& a, const Vector2D& b) {
	float x = a.m00 * b.X + a.m01 * b.Y + a.m02;   // z assumed = 1, so m02 * 1 = m02
	float y = a.m10 * b.X + a.m11 * b.Y + a.m12;   // likewise m12 contributes directly
	return Vector2D(x, y);
}

/**
 * @brief Divides every element of a matrix by a scalar.
 *        Uses reciprocal multiplication to perform a single division.
 * @param matrix The matrix whose elements are to be divided.
 * @param scalar The divisor applied uniformly to every element.
 * @return A new Matrix3x3 with each element divided by scalar.
 */
Matrix3x3 operator/(const Matrix3x3& matrix, float scalar) {
	float invScalar = 1.0f / scalar;
	return matrix * invScalar;
}

/**
 * @brief Tests exact element-wise equality between two matrices.
 *        Not epsilon-safe; use carefully with accumulated transforms.
 * @param a The left-hand matrix.
 * @param b The right-hand matrix.
 * @return True if every corresponding element is exactly equal, false otherwise.
 */
bool operator==(const Matrix3x3& a, const Matrix3x3& b) {
	return
		(a.m00 == b.m00) && (a.m01 == b.m01) && (a.m02 == b.m02) &&
		(a.m10 == b.m10) && (a.m11 == b.m11) && (a.m12 == b.m12) &&
		(a.m20 == b.m20) && (a.m21 == b.m21) && (a.m22 == b.m22);
}

/**
 * @brief Tests element-wise inequality between two matrices.
 * @param a The left-hand matrix.
 * @param b The right-hand matrix.
 * @return True if any corresponding element differs, false if all are equal.
 */
bool operator!=(const Matrix3x3& a, const Matrix3x3& b) { return !(a == b); }
