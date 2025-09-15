#include "Math/Matrix3x3.h"

// Constructors
Matrix3x3::Matrix3x3() : 
	m00(0.0f), m01(0.0f), m02(0.0f), 
	m10(0.0f), m11(0.0f), m12(0.0f), 
	m20(0.0f), m21(0.0f), m22(0.0f) {}

Matrix3x3::Matrix3x3(const float* pArr) :
	m00(pArr[0]), m01(pArr[1]), m02(pArr[2]),
	m10(pArr[3]), m11(pArr[4]), m12(pArr[5]),
	m20(pArr[6]), m21(pArr[7]), m22(pArr[8]) {}

Matrix3x3::Matrix3x3(float m00, float m01, float m02, 
	                 float m10, float m11, float m12, 
	                 float m20, float m21, float m22) :
	m00(m00), m01(m01), m02(m02),
	m10(m10), m11(m11), m12(m12),
	m20(m20), m21(m21), m22(m22) {}

// Assignment operators (member functions)
Matrix3x3& Matrix3x3::operator*=(const Matrix3x3& other) {
	// Avoid overwriting during calculation
	float temp00 = m00 * other.m00 + m01 * other.m10 + m02 * other.m20;
	float temp01 = m00 * other.m01 + m01 * other.m11 + m02 * other.m21;
	float temp02 = m00 * other.m02 + m01 * other.m12 + m02 * other.m22;

	float temp10 = m10 * other.m00 + m11 * other.m10 + m12 * other.m20;
	float temp11 = m10 * other.m01 + m11 * other.m11 + m12 * other.m21;
	float temp12 = m10 * other.m02 + m11 * other.m12 + m12 * other.m22;

	float temp20 = m20 * other.m00 + m21 * other.m10 + m22 * other.m20;
	float temp21 = m20 * other.m01 + m21 * other.m11 + m22 * other.m21;
	float temp22 = m20 * other.m02 + m21 * other.m12 + m22 * other.m22;

	// Assign
	m00 = temp00; m01 = temp01; m02 = temp02;
	m10 = temp10; m11 = temp11; m12 = temp12;
	m20 = temp20; m21 = temp21; m22 = temp22;

	return *this;
}

Matrix3x3& Matrix3x3::operator+=(const Matrix3x3& other) {
	m00 += other.m00; m01 += other.m01; m02 += other.m02;
	m10 += other.m10; m11 += other.m11; m12 += other.m12;
	m20 += other.m20; m21 += other.m21; m22 += other.m22;
	return *this;
}

Matrix3x3& Matrix3x3::operator-=(const Matrix3x3& other) {
	m00 -= other.m00; m01 -= other.m01; m02 -= other.m02;
	m10 -= other.m10; m11 -= other.m11; m12 -= other.m12;
	m20 -= other.m20; m21 -= other.m21; m22 -= other.m22;
	return *this;
}

Matrix3x3& Matrix3x3::operator*=(float scalar) {
	m00 *= scalar; m01 *= scalar; m02 *= scalar;
	m10 *= scalar; m11 *= scalar; m12 *= scalar;
	m20 *= scalar; m21 *= scalar; m22 *= scalar;
	return *this;
}

Matrix3x3& Matrix3x3::operator/=(float scalar) {
	float invScalar = 1.0f / scalar;
	m00 *= invScalar; m01 *= invScalar; m02 *= invScalar;
	m10 *= invScalar; m11 *= invScalar; m12 *= invScalar;
	m20 *= invScalar; m21 *= invScalar; m22 *= invScalar;
	return *this;
}

// Unary operator
Matrix3x3 Matrix3x3::operator-() const {
	return Matrix3x3(
		-m00, -m01, -m02,
		-m10, -m11, -m12,
		-m20, -m21, -m22
	);
}

// Matrix operations
Matrix3x3 Matrix3x3::Identity() {
	return Matrix3x3(
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	);
}

Matrix3x3 Matrix3x3::Translation(float x, float y) {
	return Matrix3x3(
		1.0f, 0.0f, x,
		0.0f, 1.0f, y,
		0.0f, 0.0f, 1.0f
	);
}

Matrix3x3 Matrix3x3::Scaling(float x, float y) {
	return Matrix3x3(
		x, 0.0f, 0.0f,
		0.0f, y, 0.0f,
		0.0f, 0.0f, 1.0f
	);
}

Matrix3x3 Matrix3x3::RotationRad(float angle) {
	float cosA = cosf(angle);
	float sinA = sinf(angle);

	return Matrix3x3(
		cosA, -sinA, 0.0f,
		sinA, cosA, 0.0f,
		0.0f, 0.0f, 1.0f
	);
}

Matrix3x3 Matrix3x3::RotationDeg(float angle) {
	// pi rad = 180 deg
	// pi / 180 rad = 1 deg
	float angleRad = angle * DEG_TO_RAD;
	return RotationRad(angleRad);
}

Matrix3x3 Matrix3x3::Transpose() const {
	return Matrix3x3(
		m00, m10, m20,
		m01, m11, m21,
		m02, m12, m22
	);
}

Matrix3x3 Matrix3x3::Inverse(float* determinant) const {
	/* Matrix format:
		float m00, m01, m02;
		float m10, m11, m12;
		float m20, m21, m22;
	*/

	// Calculate determinant
	float det = m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);

	// Return determinant if requested
	if (determinant) *determinant = det;

	// If determinant is zero, matrix is not invertible
	if (det == 0.0f) return Matrix3x3(); // Return zero matrix or identity?
	float invDet = 1.0f / det;

	// Calculate adjugate matrix (transpose of cofactor matrix)
	return Matrix3x3(
		// First row
		(m11 * m22 - m12 * m21) * invDet,
		(m02 * m21 - m01 * m22) * invDet,
		(m01 * m12 - m02 * m11) * invDet,

		// Second row
		(m12 * m20 - m10 * m22) * invDet,
		(m00 * m22 - m02 * m20) * invDet,
		(m02 * m10 - m00 * m12) * invDet,

		// Third row
		(m10 * m21 - m11 * m20) * invDet,
		(m01 * m20 - m00 * m21) * invDet,
		(m00 * m11 - m01 * m10) * invDet
	);
}

// Binary operators (non-member functions)
Matrix3x3 operator+(const Matrix3x3& a, const Matrix3x3& b) {
	return Matrix3x3(
		a.m00 + b.m00, a.m01 + b.m01, a.m02 + b.m02,
		a.m10 + b.m10, a.m11 + b.m11, a.m12 + b.m12,
		a.m20 + b.m20, a.m21 + b.m21, a.m22 + b.m22
	);
}

Matrix3x3 operator-(const Matrix3x3& a, const Matrix3x3& b) {
	return Matrix3x3(
		a.m00 - b.m00, a.m01 - b.m01, a.m02 - b.m02,
		a.m10 - b.m10, a.m11 - b.m11, a.m12 - b.m12,
		a.m20 - b.m20, a.m21 - b.m21, a.m22 - b.m22
	);
}

Matrix3x3 operator*(const Matrix3x3& matrix, float scalar) {
	return Matrix3x3(
		matrix.m00 * scalar, matrix.m01 * scalar, matrix.m02 * scalar,
		matrix.m10 * scalar, matrix.m11 * scalar, matrix.m12 * scalar,
		matrix.m20 * scalar, matrix.m21 * scalar, matrix.m22 * scalar
	);
}

Matrix3x3 operator*(float scalar, const Matrix3x3& matrix) { return matrix * scalar; }

Matrix3x3 operator*(const Matrix3x3& a, const Matrix3x3& b) {
	return Matrix3x3(
		// First row
		a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20,
		a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21,
		a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22,

		// Second row
		a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20,
		a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21,
		a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22,

		// Third row
		a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20,
		a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21,
		a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22
	);
}

Vector2D operator*(const Matrix3x3& a, const Vector2D& b) {
	// For 2D vectors, we assume z = 1 (affine transformation)
	float x = a.m00 * b.x + a.m01 * b.y + a.m02;
	float y = a.m10 * b.x + a.m11 * b.y + a.m12;
	return Vector2D(x, y);
}

Matrix3x3 operator/(const Matrix3x3& matrix, float scalar) {
	float invScalar = 1.0f / scalar;
	return matrix * invScalar; 
}

bool operator==(const Matrix3x3& a, const Matrix3x3& b) {
	return (a.m00 == b.m00) && (a.m01 == b.m01) && (a.m02 == b.m02) &&
		   (a.m10 == b.m10) && (a.m11 == b.m11) && (a.m12 == b.m12) &&
		   (a.m20 == b.m20) && (a.m21 == b.m21) && (a.m22 == b.m22);
}

bool operator!=(const Matrix3x3& a, const Matrix3x3& b) { return !(a == b); }
