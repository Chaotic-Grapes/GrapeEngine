#pragma once
#include <cmath>  // For trigonometric or arithmetic operations
#include "math/Vector2D.h"
#include "Export.h"

class GRAPEENGINE_API Matrix3x3 {
public:
	float m00, m01, m02;
	float m10, m11, m12;
	float m20, m21, m22;

	// Constructors
	Matrix3x3();
	Matrix3x3(const float* pArr);
	Matrix3x3(float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22);
	Matrix3x3(const Matrix3x3& other) = default;
	Matrix3x3& operator=(const Matrix3x3& other) = default;

	// Assignment operators (member functions)
	Matrix3x3& operator*=(const Matrix3x3& other);
	Matrix3x3& operator+=(const Matrix3x3& other);
	Matrix3x3& operator-=(const Matrix3x3& other);
	Matrix3x3& operator*=(float scalar);
	Matrix3x3& operator/=(float scalar);

	// Unary operator
	Matrix3x3 operator-() const;

	// Matrix operations
	static Matrix3x3 Identity();
	static Matrix3x3 Translation(float x, float y);
	static Matrix3x3 Scaling(float x, float y);
	static Matrix3x3 RotationRad(float angle);
	static Matrix3x3 RotationDeg(float angle);
	Matrix3x3 Transpose() const;
	Matrix3x3 Inverse(float* determinant = nullptr) const;
};

// Binary operators (non-member functions)
Matrix3x3 operator+(const Matrix3x3& a, const Matrix3x3& b);
Matrix3x3 operator-(const Matrix3x3& a, const Matrix3x3& b);
Matrix3x3 operator*(const Matrix3x3& matrix, float scalar);
Matrix3x3 operator*(float scalar, const Matrix3x3& matrix);
Matrix3x3 operator*(const Matrix3x3& a, const Matrix3x3& b);
Vector2D operator*(const Matrix3x3& a, const Vector2D& b);
Matrix3x3 operator/(const Matrix3x3& matrix, float scalar);
bool operator==(const Matrix3x3& a, const Matrix3x3& b);
bool operator!=(const Matrix3x3& a, const Matrix3x3& b);

// Constants
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;