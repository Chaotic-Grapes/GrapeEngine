#include "Math/Vector4D.h"
#include <cmath>  // For trigonometric or arithmetic operations
#include <stdexcept>

// Constructors
Vector4D::Vector4D() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
Vector4D::Vector4D(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

// Assignment operators (member functions)
Vector4D& Vector4D::operator+=(const Vector4D& other) {
	x += other.x;
	y += other.y;
	z += other.z;
	w += other.w;
	return *this;
}

Vector4D& Vector4D::operator-=(const Vector4D& other) {
	x -= other.x;
	y -= other.y;
	z -= other.z;
	w -= other.w;
	return *this;
}

Vector4D& Vector4D::operator*=(float scalar) {
	x *= scalar;
	y *= scalar;
	z *= scalar;
	w *= scalar;
	return *this;
}

Vector4D& Vector4D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	x *= invScalar;
	y *= invScalar;
	z *= invScalar;
	w *= invScalar;
	return *this;
}

// Unary operator
Vector4D Vector4D::operator-() const { return Vector4D(-x, -y, -z, -w); }

// Member functions
float Vector4D::Length() const { return sqrtf(x * x + y * y + z * z + w * w); }
float Vector4D::SquareLength() const { return x * x + y * y + z * z + w * w; }

// Returns a new vector
Vector4D Vector4D::Normalized() const {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector4D(x * invLength, y * invLength, z * invLength, w * invLength);
	}
	return Vector4D(0.0f, 0.0f, 0.0f, 0.0f);
}

// Modifies the current vector
void Vector4D::Normalize() {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		x *= invLength;
		y *= invLength;
		z *= invLength;
		w *= invLength;
	}
}

// Easier access
float Vector4D::Dot(const Vector4D& other) const { return x * other.x + y * other.y + z * other.z + w * other.w; }

// Static functions
float Vector4D::Dot(const Vector4D& a, const Vector4D& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

float Vector4D::Distance(const Vector4D& a, const Vector4D& b) {
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	float dw = a.w - b.w;
	return sqrtf(dx * dx + dy * dy + dz * dz + dw * dw);
}

float Vector4D::SquareDistance(const Vector4D& a, const Vector4D& b) {
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	float dw = a.w - b.w;
	return dx * dx + dy * dy + dz * dz + dw * dw;
}

Vector4D Vector4D::Lerp(const Vector4D& a, const Vector4D& b, float t) {
	// lerp = a + (b - a) t; clamp t between 0 and 1
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector4D(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

Vector4D Vector4D::Homogenize(const Vector4D& vector) { 
	if (vector.w != 0.0f) {
		float invW = 1.0f / vector.w;
		return Vector4D(
			vector.x * invW,
			vector.y * invW,
			vector.z * invW,
			1.0f
		);
	}
	return vector;
}

Vector4D Vector4D::ClampVector(const Vector4D& vector, const Vector4D& min, const Vector4D& max) {
	// Clamp each component individually
	float clampedX = (vector.x < min.x) ? min.x : ((vector.x > max.x) ? max.x : vector.x);
	float clampedY = (vector.y < min.y) ? min.y : ((vector.y > max.y) ? max.y : vector.y);
	float clampedZ = (vector.z < min.z) ? min.z : ((vector.z > max.z) ? max.z : vector.z);
	float clampedW = (vector.w < min.w) ? min.w : ((vector.w > max.w) ? max.w : vector.w);
	return Vector4D(clampedX, clampedY, clampedZ, clampedW);
}

float Vector4D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// If w == 1.0f
bool Vector4D::IsPoint() const { return w == 1.0f; }
// If w == 0.0f
bool Vector4D::IsVector() const { return w == 0.0f; }
// Extract 3D components
Vector3D Vector4D::XYZ() const { return Vector3D(x, y, z); }

// Binary operators (non-member functions)
Vector4D operator+(const Vector4D& a, const Vector4D& b) { return Vector4D(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
Vector4D operator-(const Vector4D& a, const Vector4D& b) { return Vector4D(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
Vector4D operator*(const Vector4D& vector, float scalar) { return Vector4D(vector.x * scalar, vector.y * scalar, vector.z * scalar, vector.w * scalar); }
Vector4D operator*(float scalar, const Vector4D& vector) { return Vector4D(vector.x * scalar, vector.y * scalar, vector.z * scalar, vector.w * scalar); }

Vector4D operator/(const Vector4D& vector, float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector4D(vector.x * invScalar, vector.y * invScalar, vector.z * invScalar, vector.w * invScalar);
}

bool operator==(const Vector4D& a, const Vector4D& b) { return (a.x == b.x) && (a.y == b.y) && (a.z == b.z) && (a.w == b.w); }
bool operator!=(const Vector4D& a, const Vector4D& b) { return !(a == b); }
