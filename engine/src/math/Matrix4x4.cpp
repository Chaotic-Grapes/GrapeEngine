/* Start Header *****************************************************************/
/*!
\file   Matrix4x4.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
4x4 matrix math implementation. Covers construction, compound assignment operators,
element-wise and matrix multiplication, static factory methods for 3D transforms
(translation, scale, axis rotations), orthographic and perspective projection,
LookAt view matrix, transpose, inverse via cofactor/adjugate expansion,
and non-member binary operators including Matrix4x4 * Vector3D and Matrix4x4 * Vector4D.
Matrix is stored row-major: mRC where R is the row index and C is the column index.
*/
/* End Header *******************************************************************/

#include "math/Matrix4x4.h"

// ============================================================================
// Constructors
// ============================================================================

// Default-construct to the zero matrix; callers needing identity should use Matrix4x4::Identity()
Matrix4x4::Matrix4x4() :
	m00(0.0f), m01(0.0f), m02(0.0f), m03(0.0f),
	m10(0.0f), m11(0.0f), m12(0.0f), m13(0.0f),
	m20(0.0f), m21(0.0f), m22(0.0f), m23(0.0f),
	m30(0.0f), m31(0.0f), m32(0.0f), m33(0.0f) {
}

// Construct from a flat array in row-major order: 16 elements, first row first
Matrix4x4::Matrix4x4(const float* pArr) :
	m00(pArr[0]), m01(pArr[1]), m02(pArr[2]), m03(pArr[3]),
	m10(pArr[4]), m11(pArr[5]), m12(pArr[6]), m13(pArr[7]),
	m20(pArr[8]), m21(pArr[9]), m22(pArr[10]), m23(pArr[11]),
	m30(pArr[12]), m31(pArr[13]), m32(pArr[14]), m33(pArr[15]) {
}

// Construct from sixteen explicit scalars listed in row-major order
Matrix4x4::Matrix4x4(float m00, float m01, float m02, float m03,
	float m10, float m11, float m12, float m13,
	float m20, float m21, float m22, float m23,
	float m30, float m31, float m32, float m33) :
	m00(m00), m01(m01), m02(m02), m03(m03),
	m10(m10), m11(m11), m12(m12), m13(m13),
	m20(m20), m21(m21), m22(m22), m23(m23),
	m30(m30), m31(m31), m32(m32), m33(m33) {
}

// ============================================================================
// Assignment operators (member functions)
// ============================================================================

// Matrix multiply-assign: result[r][c] = dot of this row r and other column c;
// all sixteen temporaries are computed before any write to avoid aliasing
Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& other) {
	// Compute all output elements into temporaries before overwriting any member,
	// because each element of *this feeds multiple dot products across the rows
	float temp00 = m00 * other.m00 + m01 * other.m10 + m02 * other.m20 + m03 * other.m30;
	float temp01 = m00 * other.m01 + m01 * other.m11 + m02 * other.m21 + m03 * other.m31;
	float temp02 = m00 * other.m02 + m01 * other.m12 + m02 * other.m22 + m03 * other.m32;
	float temp03 = m00 * other.m03 + m01 * other.m13 + m02 * other.m23 + m03 * other.m33;

	float temp10 = m10 * other.m00 + m11 * other.m10 + m12 * other.m20 + m13 * other.m30;
	float temp11 = m10 * other.m01 + m11 * other.m11 + m12 * other.m21 + m13 * other.m31;
	float temp12 = m10 * other.m02 + m11 * other.m12 + m12 * other.m22 + m13 * other.m32;
	float temp13 = m10 * other.m03 + m11 * other.m13 + m12 * other.m23 + m13 * other.m33;

	float temp20 = m20 * other.m00 + m21 * other.m10 + m22 * other.m20 + m23 * other.m30;
	float temp21 = m20 * other.m01 + m21 * other.m11 + m22 * other.m21 + m23 * other.m31;
	float temp22 = m20 * other.m02 + m21 * other.m12 + m22 * other.m22 + m23 * other.m32;
	float temp23 = m20 * other.m03 + m21 * other.m13 + m22 * other.m23 + m23 * other.m33;

	float temp30 = m30 * other.m00 + m31 * other.m10 + m32 * other.m20 + m33 * other.m30;
	float temp31 = m30 * other.m01 + m31 * other.m11 + m32 * other.m21 + m33 * other.m31;
	float temp32 = m30 * other.m02 + m31 * other.m12 + m32 * other.m22 + m33 * other.m32;
	float temp33 = m30 * other.m03 + m31 * other.m13 + m32 * other.m23 + m33 * other.m33;

	// Safe to write back now that all reads are done
	m00 = temp00; m01 = temp01; m02 = temp02; m03 = temp03;
	m10 = temp10; m11 = temp11; m12 = temp12; m13 = temp13;
	m20 = temp20; m21 = temp21; m22 = temp22; m23 = temp23;
	m30 = temp30; m31 = temp31; m32 = temp32; m33 = temp33;

	return *this;
}

// Element-wise addition
Matrix4x4& Matrix4x4::operator+=(const Matrix4x4& other) {
	m00 += other.m00; m01 += other.m01; m02 += other.m02; m03 += other.m03;
	m10 += other.m10; m11 += other.m11; m12 += other.m12; m13 += other.m13;
	m20 += other.m20; m21 += other.m21; m22 += other.m22; m23 += other.m23;
	m30 += other.m30; m31 += other.m31; m32 += other.m32; m33 += other.m33;
	return *this;
}

// Element-wise subtraction
Matrix4x4& Matrix4x4::operator-=(const Matrix4x4& other) {
	m00 -= other.m00; m01 -= other.m01; m02 -= other.m02; m03 -= other.m03;
	m10 -= other.m10; m11 -= other.m11; m12 -= other.m12; m13 -= other.m13;
	m20 -= other.m20; m21 -= other.m21; m22 -= other.m22; m23 -= other.m23;
	m30 -= other.m30; m31 -= other.m31; m32 -= other.m32; m33 -= other.m33;
	return *this;
}

// Uniform scale of all elements by a scalar
Matrix4x4& Matrix4x4::operator*=(float scalar) {
	m00 *= scalar; m01 *= scalar; m02 *= scalar; m03 *= scalar;
	m10 *= scalar; m11 *= scalar; m12 *= scalar; m13 *= scalar;
	m20 *= scalar; m21 *= scalar; m22 *= scalar; m23 *= scalar;
	m30 *= scalar; m31 *= scalar; m32 *= scalar; m33 *= scalar;
	return *this;
}

// Uniform division; multiply by reciprocal to do one division instead of sixteen
Matrix4x4& Matrix4x4::operator/=(float scalar) {
	float invScalar = 1.0f / scalar;
	m00 *= invScalar; m01 *= invScalar; m02 *= invScalar; m03 *= invScalar;
	m10 *= invScalar; m11 *= invScalar; m12 *= invScalar; m13 *= invScalar;
	m20 *= invScalar; m21 *= invScalar; m22 *= invScalar; m23 *= invScalar;
	m30 *= invScalar; m31 *= invScalar; m32 *= invScalar; m33 *= invScalar;
	return *this;
}

// ============================================================================
// Unary operator
// ============================================================================

// Return a copy with all elements negated; useful for reversing a signed offset matrix
Matrix4x4 Matrix4x4::operator-() const {
	return Matrix4x4(
		-m00, -m01, -m02, -m03,
		-m10, -m11, -m12, -m13,
		-m20, -m21, -m22, -m23,
		-m30, -m31, -m32, -m33
	);
}

// ============================================================================
// Matrix operations
// ============================================================================

// Return the multiplicative identity; transforms are built by multiplying against this
Matrix4x4 Matrix4x4::Identity() {
	return Matrix4x4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

// 3D affine translation matrix; offset is packed into the fourth column so that
// multiplying a homogeneous point (W = 1) adds the translation to XYZ
Matrix4x4 Matrix4x4::Translation(float x, float y, float z) {
	return Matrix4x4(
		1.0f, 0.0f, 0.0f, x,
		0.0f, 1.0f, 0.0f, y,
		0.0f, 0.0f, 1.0f, z,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

// 3D non-uniform scale matrix; scales each axis independently
Matrix4x4 Matrix4x4::Scaling(float x, float y, float z) {
	return Matrix4x4(
		x, 0.0f, 0.0f, 0.0f,
		0.0f, y, 0.0f, 0.0f,
		0.0f, 0.0f, z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

// CCW rotation around the world X axis by angle in radians:
// [ 1    0     0   0 ]
// [ 0   cos  -sin  0 ]
// [ 0   sin   cos  0 ]
// [ 0    0     0   1 ]
Matrix4x4 Matrix4x4::RotationX(float angle) {
	float cosA = std::cos(angle);
	float sinA = std::sin(angle);

	return Matrix4x4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, cosA, -sinA, 0.0f,
		0.0f, sinA, cosA, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

// CCW rotation around the world Y axis by angle in radians;
// note the transposed sin positions relative to RotationX — this maintains right-handedness:
// [  cos  0  sin  0 ]
// [   0   1   0   0 ]
// [ -sin  0  cos  0 ]
// [   0   0   0   1 ]
Matrix4x4 Matrix4x4::RotationY(float angle) {
	float cosA = std::cos(angle);
	float sinA = std::sin(angle);

	return Matrix4x4(
		cosA, 0.0f, sinA, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sinA, 0.0f, cosA, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

// CCW rotation around the world Z axis by angle in radians:
// [ cos  -sin  0  0 ]
// [ sin   cos  0  0 ]
// [  0     0   1  0 ]
// [  0     0   0  1 ]
Matrix4x4 Matrix4x4::RotationZ(float angle) {
	float cosA = std::cos(angle);
	float sinA = std::sin(angle);

	return Matrix4x4(
		cosA, -sinA, 0.0f, 0.0f,
		sinA, cosA, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

// ============================================================================
// Projection matrices
// ============================================================================

// Orthographic projection: maps the axis-aligned box [left,right] x [bottom,top] x [near,far]
// to the NDC cube [-1,1]^3; the XY scale factors are 2/width and 2/height, the Z scale is
// -2/depth (negated because the view frustum goes into -Z), and the translation terms
// shift each axis midpoint to the origin
Matrix4x4 Matrix4x4::Orthographic(float left, float right, float bottom, float top, float near, float far) {
	float rl = right - left;   // frustum width
	float tb = top - bottom;   // frustum height
	float fn = far - near;     // frustum depth

	return Matrix4x4(
		2.0f / rl, 0.0f, 0.0f, -(right + left) / rl,
		0.0f, 2.0f / tb, 0.0f, -(top + bottom) / tb,
		0.0f, 0.0f, -2.0f / fn, -(far + near) / fn,
		0.0f, 0.0f, 0.0f, 1.0f
	);
}

// Perspective projection: maps the view frustum to NDC using the pinhole camera model;
// fov is the full vertical field-of-view in radians, aspect is width/height;
// the [2][2] and [2][3] entries encode the non-linear depth remapping so that near maps
// to -1 and far maps to +1 in NDC, and [3][2] = -1 performs the perspective divide by -Z
Matrix4x4 Matrix4x4::Perspective(float fov, float aspect, float near, float far) {
	float tanHalfFov = tanf(fov / 2.0f);   // half-height of the near plane at distance 1
	float fn = far - near;                 // depth range for the Z remapping terms

	return Matrix4x4(
		1.0f / (aspect * tanHalfFov), 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / tanHalfFov, 0.0f, 0.0f,
		0.0f, 0.0f, -(far + near) / fn, -2.0f * far * near / fn,
		0.0f, 0.0f, -1.0f, 0.0f
	);
}

// ============================================================================
// View matrix
// ============================================================================

// Build a view matrix from world-space camera position, target and up hint;
// the view matrix is the inverse of the camera's world transform, so it
// rotates world space into camera space and translates by the negated eye position
Matrix4x4 Matrix4x4::LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up) {
	// Forward points from eye toward target; this becomes the camera's -Z in view space
	Vector3D forward = (target - eye).Normalized();

	// Right is perpendicular to both up and forward; cross(up, forward) gives the camera's +X axis;
	// note the argument order: cross(up, forward) not cross(forward, up) to get the correct handedness
	Vector3D right = Vector3D::Cross(up, forward).Normalized();

	// Recompute up orthogonally from the now-final forward and right axes; the hint up vector
	// is not guaranteed to be perpendicular to forward, so this corrects any skew
	Vector3D newUp = Vector3D::Cross(forward, right);

	// Pack the three basis vectors as rows; this is the rotation-only part of the view matrix
	// (transposing a rotation matrix inverts it, which is what we want to go from world to camera)
	Matrix4x4 rotation(
		right.X, right.Y, right.Z, 0.0f,
		newUp.X, newUp.Y, newUp.Z, 0.0f,
		forward.X, forward.Y, forward.Z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	// Translate by the negated eye position to move world geometry into camera-relative space
	Matrix4x4 translation = Matrix4x4::Translation(-eye.X, -eye.Y, -eye.Z);

	// View = Rotation * Translation; rotation must come first so that the translation
	// is expressed in the rotated (camera) basis rather than world basis
	return rotation * translation;
}

// Transpose: swap rows and columns; for pure rotation matrices, transpose == inverse
Matrix4x4 Matrix4x4::Transpose() const {
	return Matrix4x4(
		m00, m10, m20, m30,
		m01, m11, m21, m31,
		m02, m12, m22, m32,
		m03, m13, m23, m33
	);
}

// Compute the inverse via the classical adjugate formula: A^-1 = adj(A) / det(A);
// the adjugate is the transpose of the cofactor matrix where each cofactor is the
// signed 3x3 minor of the element at the transposed position;
// if the determinant is zero the matrix is singular and the zero matrix is returned
Matrix4x4 Matrix4x4::Inverse(float* determinant) const {
	// Expand along the first row; each term is a signed 3x3 minor multiplied by its m0c element;
	// the 3x3 minors are themselves expanded inline as cofactor triples
	float det =
		m00 * (m11 * (m22 * m33 - m23 * m32) - m12 * (m21 * m33 - m23 * m31) + m13 * (m21 * m32 - m22 * m31)) -
		m01 * (m10 * (m22 * m33 - m23 * m32) - m12 * (m20 * m33 - m23 * m30) + m13 * (m20 * m32 - m22 * m30)) +
		m02 * (m10 * (m21 * m33 - m23 * m31) - m11 * (m20 * m33 - m23 * m30) + m13 * (m20 * m31 - m21 * m30)) -
		m03 * (m10 * (m21 * m32 - m22 * m31) - m11 * (m20 * m32 - m22 * m30) + m12 * (m20 * m31 - m21 * m30));

	// Write out the determinant if the caller wants it (e.g. to detect singularity)
	if (determinant) *determinant = det;

	// Singular matrix has no inverse; return zero matrix as a safe fallback
	if (det == 0.0f) return Matrix4x4();

	float invDet = 1.0f / det;

	// Each output element is the cofactor of the *transposed* position scaled by invDet;
	// sign is (-1)^(r+c) and the 2x2 minor is taken from the rows/columns not in the original r/c;
	// the pattern follows directly from the adjugate definition: adj(A)[r][c] = C[c][r]
	return Matrix4x4(
		// Row 0
		(m11 * (m22 * m33 - m23 * m32) - m12 * (m21 * m33 - m23 * m31) + m13 * (m21 * m32 - m22 * m31)) * invDet,
		(-m01 * (m22 * m33 - m23 * m32) + m02 * (m21 * m33 - m23 * m31) - m03 * (m21 * m32 - m22 * m31)) * invDet,
		(m01 * (m12 * m33 - m13 * m32) - m02 * (m11 * m33 - m13 * m31) + m03 * (m11 * m32 - m12 * m31)) * invDet,
		(-m01 * (m12 * m23 - m13 * m22) + m02 * (m11 * m23 - m13 * m21) - m03 * (m11 * m22 - m12 * m21)) * invDet,

		// Row 1
		(-m10 * (m22 * m33 - m23 * m32) + m12 * (m20 * m33 - m23 * m30) - m13 * (m20 * m32 - m22 * m30)) * invDet,
		(m00 * (m22 * m33 - m23 * m32) - m02 * (m20 * m33 - m23 * m30) + m03 * (m20 * m32 - m22 * m30)) * invDet,
		(-m00 * (m12 * m33 - m13 * m32) + m02 * (m10 * m33 - m13 * m30) - m03 * (m10 * m32 - m12 * m30)) * invDet,
		(m00 * (m12 * m23 - m13 * m22) - m02 * (m10 * m23 - m13 * m20) + m03 * (m10 * m22 - m12 * m20)) * invDet,

		// Row 2
		(m10 * (m21 * m33 - m23 * m31) - m11 * (m20 * m33 - m23 * m30) + m13 * (m20 * m31 - m21 * m30)) * invDet,
		(-m00 * (m21 * m33 - m23 * m31) + m01 * (m20 * m33 - m23 * m30) - m03 * (m20 * m31 - m21 * m30)) * invDet,
		(m00 * (m11 * m33 - m13 * m31) - m01 * (m10 * m33 - m13 * m30) + m03 * (m10 * m31 - m11 * m30)) * invDet,
		(-m00 * (m11 * m23 - m13 * m21) + m01 * (m10 * m23 - m13 * m20) - m03 * (m10 * m21 - m11 * m20)) * invDet,

		// Row 3
		(-m10 * (m21 * m32 - m22 * m31) + m11 * (m20 * m32 - m22 * m30) - m12 * (m20 * m31 - m21 * m30)) * invDet,
		(m00 * (m21 * m32 - m22 * m31) - m01 * (m20 * m32 - m22 * m30) + m02 * (m20 * m31 - m21 * m30)) * invDet,
		(-m00 * (m11 * m32 - m12 * m31) + m01 * (m10 * m32 - m12 * m30) - m02 * (m10 * m31 - m11 * m30)) * invDet,
		(m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20)) * invDet
	);
}

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

// Element-wise addition
Matrix4x4 operator+(const Matrix4x4& a, const Matrix4x4& b) {
	return Matrix4x4(
		a.m00 + b.m00, a.m01 + b.m01, a.m02 + b.m02, a.m03 + b.m03,
		a.m10 + b.m10, a.m11 + b.m11, a.m12 + b.m12, a.m13 + b.m13,
		a.m20 + b.m20, a.m21 + b.m21, a.m22 + b.m22, a.m23 + b.m23,
		a.m30 + b.m30, a.m31 + b.m31, a.m32 + b.m32, a.m33 + b.m33
	);
}

// Element-wise subtraction
Matrix4x4 operator-(const Matrix4x4& a, const Matrix4x4& b) {
	return Matrix4x4(
		a.m00 - b.m00, a.m01 - b.m01, a.m02 - b.m02, a.m03 - b.m03,
		a.m10 - b.m10, a.m11 - b.m11, a.m12 - b.m12, a.m13 - b.m13,
		a.m20 - b.m20, a.m21 - b.m21, a.m22 - b.m22, a.m23 - b.m23,
		a.m30 - b.m30, a.m31 - b.m31, a.m32 - b.m32, a.m33 - b.m33
	);
}

// Scalar multiplication; each element scaled uniformly
Matrix4x4 operator*(const Matrix4x4& matrix, float scalar) {
	return Matrix4x4(
		matrix.m00 * scalar, matrix.m01 * scalar, matrix.m02 * scalar, matrix.m03 * scalar,
		matrix.m10 * scalar, matrix.m11 * scalar, matrix.m12 * scalar, matrix.m13 * scalar,
		matrix.m20 * scalar, matrix.m21 * scalar, matrix.m22 * scalar, matrix.m23 * scalar,
		matrix.m30 * scalar, matrix.m31 * scalar, matrix.m32 * scalar, matrix.m33 * scalar
	);
}

// Commutative scalar overload so scalar * matrix also works
Matrix4x4 operator*(float scalar, const Matrix4x4& matrix) { return matrix * scalar; }

// Full 4x4 matrix multiplication: result[r][c] = dot(row r of a, column c of b)
Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b) {
	return Matrix4x4(
		// Row 0
		a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20 + a.m03 * b.m30,
		a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21 + a.m03 * b.m31,
		a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22 + a.m03 * b.m32,
		a.m00 * b.m03 + a.m01 * b.m13 + a.m02 * b.m23 + a.m03 * b.m33,

		// Row 1
		a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20 + a.m13 * b.m30,
		a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21 + a.m13 * b.m31,
		a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22 + a.m13 * b.m32,
		a.m10 * b.m03 + a.m11 * b.m13 + a.m12 * b.m23 + a.m13 * b.m33,

		// Row 2
		a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20 + a.m23 * b.m30,
		a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21 + a.m23 * b.m31,
		a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22 + a.m23 * b.m32,
		a.m20 * b.m03 + a.m21 * b.m13 + a.m22 * b.m23 + a.m23 * b.m33,

		// Row 3
		a.m30 * b.m00 + a.m31 * b.m10 + a.m32 * b.m20 + a.m33 * b.m30,
		a.m30 * b.m01 + a.m31 * b.m11 + a.m32 * b.m21 + a.m33 * b.m31,
		a.m30 * b.m02 + a.m31 * b.m12 + a.m32 * b.m22 + a.m33 * b.m32,
		a.m30 * b.m03 + a.m31 * b.m13 + a.m32 * b.m23 + a.m33 * b.m33
	);
}

// ============================================================================
// Vector transformations
// ============================================================================

// Transform a 3D point by implicitly extending it to homogeneous (X, Y, Z, W=1),
// so the translation column (m03, m13, m23) is fully applied; W is discarded
// since we return a Vector3D — caller should use Vector4D if W matters
Vector3D operator*(const Matrix4x4& m, const Vector3D& v) {
	float x = m.m00 * v.X + m.m01 * v.Y + m.m02 * v.Z + m.m03;   // W = 1, so m03 * 1 = m03
	float y = m.m10 * v.X + m.m11 * v.Y + m.m12 * v.Z + m.m13;
	float z = m.m20 * v.X + m.m21 * v.Y + m.m22 * v.Z + m.m23;
	return Vector3D(x, y, z);
}

// Full homogeneous transform; W is preserved so callers can detect perspective division
// or distinguish points (W=1) from directions (W=0) after the multiply
Vector4D operator*(const Matrix4x4& m, const Vector4D& v) {
	float x = m.m00 * v.X + m.m01 * v.Y + m.m02 * v.Z + m.m03 * v.W;
	float y = m.m10 * v.X + m.m11 * v.Y + m.m12 * v.Z + m.m13 * v.W;
	float z = m.m20 * v.X + m.m21 * v.Y + m.m22 * v.Z + m.m23 * v.W;
	float w = m.m30 * v.X + m.m31 * v.Y + m.m32 * v.Z + m.m33 * v.W;
	return Vector4D(x, y, z, w);
}

// Scalar division; multiply by reciprocal then reuse the scalar * matrix path
Matrix4x4 operator/(const Matrix4x4& matrix, float scalar) {
	float invScalar = 1.0f / scalar;
	return Matrix4x4(
		matrix.m00 * invScalar, matrix.m01 * invScalar, matrix.m02 * invScalar, matrix.m03 * invScalar,
		matrix.m10 * invScalar, matrix.m11 * invScalar, matrix.m12 * invScalar, matrix.m13 * invScalar,
		matrix.m20 * invScalar, matrix.m21 * invScalar, matrix.m22 * invScalar, matrix.m23 * invScalar,
		matrix.m30 * invScalar, matrix.m31 * invScalar, matrix.m32 * invScalar, matrix.m33 * invScalar
	);
}

// Exact element-wise equality; not epsilon-safe, use carefully with accumulated transforms
bool operator==(const Matrix4x4& a, const Matrix4x4& b) {
	return
		(a.m00 == b.m00) && (a.m01 == b.m01) && (a.m02 == b.m02) && (a.m03 == b.m03) &&
		(a.m10 == b.m10) && (a.m11 == b.m11) && (a.m12 == b.m12) && (a.m13 == b.m13) &&
		(a.m20 == b.m20) && (a.m21 == b.m21) && (a.m22 == b.m22) && (a.m23 == b.m23) &&
		(a.m30 == b.m30) && (a.m31 == b.m31) && (a.m32 == b.m32) && (a.m33 == b.m33);
}

// Inequality via negated equality check
bool operator!=(const Matrix4x4& a, const Matrix4x4& b) { return !(a == b); }
