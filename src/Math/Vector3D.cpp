#include "Math/Vector3D.h"
#include <cmath> // For trigonometric or arithmetic operations
#include <stdexcept>

// Constructors
Vector3D::Vector3D() : x(0.0f), y(0.0f), z(0.0f) {}
Vector3D::Vector3D(float x, float y, float z) : x(x), y(y), z(z) {}

// Assignment operators (member functions)
Vector3D& Vector3D::operator+=(const Vector3D& other) {
	x += other.x;
	y += other.y;
	z += other.z;
	return *this;
}

Vector3D& Vector3D::operator-=(const Vector3D& other) {
	x -= other.x;
	y -= other.y;
	z -= other.z;
	return *this;
}

Vector3D& Vector3D::operator*=(float scalar) {
	x *= scalar;
	y *= scalar;
	z *= scalar;
	return *this;
}

Vector3D& Vector3D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	x *= invScalar;
	y *= invScalar;
	z *= invScalar;
	return *this;
}

// Unary operator
Vector3D Vector3D::operator-() const { return Vector3D(-x, -y, -z); }

// Member functions
float Vector3D::Length() const { return sqrtf(x * x + y * y + z * z); }
float Vector3D::SquareLength() const { return x * x + y * y + z * z; }

// Returns a new vector
Vector3D Vector3D::Normalized() const {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector3D(x * invLength, y * invLength, z * invLength);
	}
	return Vector3D(0.0f, 0.0f, 0.0f);
}

// Modifies the current vector
void Vector3D::Normalize() {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		x *= invLength;
		y *= invLength;
		z *= invLength;
	}
}

// Easier access
float Vector3D::Dot(const Vector3D& other) const { return x * other.x + y * other.y + z * other.z; }

// Static functions
float Vector3D::Dot(const Vector3D& a, const Vector3D& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vector3D Vector3D::Cross(const Vector3D& a, const Vector3D& b) {
	return Vector3D(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

float Vector3D::Distance(const Vector3D& a, const Vector3D& b) {
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

float Vector3D::SquareDistance(const Vector3D& a, const Vector3D& b) {
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	float dz = a.z - b.z;
	return dx * dx + dy * dy + dz * dz;
}

Vector3D Vector3D::Lerp(const Vector3D& a, const Vector3D& b, float t) {
	// lerp = a + (b - a) t; clamp t between 0 and 1
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector3D(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

Vector3D Vector3D::ClampVector(const Vector3D& vector, const Vector3D& min, const Vector3D& max) {
	// Clamp each component individually
	float clampedX = (vector.x < min.x) ? min.x : ((vector.x > max.x) ? max.x : vector.x);
	float clampedY = (vector.y < min.y) ? min.y : ((vector.y > max.y) ? max.y : vector.y);
	float clampedZ = (vector.z < min.z) ? min.z : ((vector.z > max.z) ? max.z : vector.z);
	return Vector3D(clampedX, clampedY, clampedZ);
}

float Vector3D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// Binary operators (non-member functions)
Vector3D operator+(const Vector3D& a, const Vector3D& b) { return Vector3D(a.x + b.x, a.y + b.y, a.z + b.z); }
Vector3D operator-(const Vector3D& a, const Vector3D& b) { return Vector3D(a.x - b.x, a.y - b.y, a.z - b.z); }
Vector3D operator*(const Vector3D& vector, float scalar) { return Vector3D(vector.x * scalar, vector.y * scalar, vector.z * scalar); }
Vector3D operator*(float scalar, const Vector3D& vector) { return Vector3D(vector.x * scalar, vector.y * scalar, vector.z * scalar); }

Vector3D operator/(const Vector3D& vector, float scalar) { 
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector3D(vector.x * invScalar, vector.y * invScalar, vector.z * invScalar);
}

bool operator==(const Vector3D& a, const Vector3D& b) { return (a.x == b.x) && (a.y == b.y) && (a.z == b.z); }  
bool operator!=(const Vector3D& a, const Vector3D& b) { return !(a == b); }
