#pragma once
#include <cmath>  // For trigonometric or arithmetic operations
#include "math/Vector3D.h"
#include "math/Vector4D.h"
#include "Export.h"

class GRAPEENGINE_API Matrix4x4 {
public:
	float m00, m01, m02, m03;
	float m10, m11, m12, m13;
	float m20, m21, m22, m23;
	float m30, m31, m32, m33;

	// Constructors
	Matrix4x4();
	Matrix4x4(const float* pArr);
	Matrix4x4(float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33);
	Matrix4x4(const Matrix4x4& other) = default;
	Matrix4x4& operator=(const Matrix4x4& other) = default;

	// Assignment operators (member functions)
	Matrix4x4& operator*=(const Matrix4x4& other);
	Matrix4x4& operator+=(const Matrix4x4& other);
	Matrix4x4& operator-=(const Matrix4x4& other);
	Matrix4x4& operator*=(float scalar);
	Matrix4x4& operator/=(float scalar);

	// Unary operator
	Matrix4x4 operator-() const;

	// Matrix operations
	static Matrix4x4 Identity();
	static Matrix4x4 Translation(float x, float y, float z);
	static Matrix4x4 Scaling(float x, float y, float z);
	static Matrix4x4 RotationX(float angle);
	static Matrix4x4 RotationY(float angle);
	static Matrix4x4 RotationZ(float angle);

	// Projection matrices
	static Matrix4x4 Orthographic(float left, float right, float bottom, float top, float near, float far);
	static Matrix4x4 Perspective(float fov, float aspect, float near, float far);

	// View matrix
	static Matrix4x4 LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up);

	Matrix4x4 Transpose() const;
	Matrix4x4 Inverse(float* determinant = nullptr) const;
};

// Binary operators (non-member functions)
GRAPEENGINE_API Matrix4x4 operator+(const Matrix4x4& a, const Matrix4x4& b);
GRAPEENGINE_API Matrix4x4 operator-(const Matrix4x4& a, const Matrix4x4& b);
GRAPEENGINE_API Matrix4x4 operator*(const Matrix4x4& matrix, float scalar);
GRAPEENGINE_API Matrix4x4 operator*(float scalar, const Matrix4x4& matrix);
GRAPEENGINE_API Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b);

// Vector transformations
GRAPEENGINE_API Vector3D operator*(const Matrix4x4& a, const Vector3D& b);
GRAPEENGINE_API Vector4D operator*(const Matrix4x4& a, const Vector4D& b);

GRAPEENGINE_API Matrix4x4 operator/(const Matrix4x4& matrix, float scalar);
GRAPEENGINE_API bool operator==(const Matrix4x4& a, const Matrix4x4& b);
bool operator!=(const Matrix4x4& a, const Matrix4x4& b);

// Constants
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;
