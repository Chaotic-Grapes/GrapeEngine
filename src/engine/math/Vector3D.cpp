#include "Math/Vector3D.h"
#include <cmath> // For trigonometric or arithmetic operations
#include <stdexcept>

// Constructors
Vector3D::Vector3D() : X(0.0f), Y(0.0f), Z(0.0f) {}
Vector3D::Vector3D(float x, float y, float z) : X(x), Y(y), Z(z) {}

// Assignment operators (member functions)
Vector3D& Vector3D::operator+=(const Vector3D& other) {
	X += other.X;
	Y += other.Y;
	Z += other.Z;
	return *this;
}

Vector3D& Vector3D::operator-=(const Vector3D& other) {
	X -= other.X;
	Y -= other.Y;
	Z -= other.Z;
	return *this;
}

Vector3D& Vector3D::operator*=(float scalar) {
	X *= scalar;
	Y *= scalar;
	Z *= scalar;
	return *this;
}

Vector3D& Vector3D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	X *= invScalar;
	Y *= invScalar;
	Z *= invScalar;
	return *this;
}

// Unary operator
Vector3D Vector3D::operator-() const { return Vector3D(-X, -Y, -Z); }

// Member functions
float Vector3D::Length() const { return sqrtf(X * X + Y * Y + Z * Z); }
float Vector3D::SquareLength() const { return X * X + Y * Y + Z * Z; }

// Returns a new vector
Vector3D Vector3D::Normalized() const {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector3D(X * invLength, Y * invLength, Z * invLength);
	}
	return Vector3D(0.0f, 0.0f, 0.0f);
}

// Modifies the current vector
void Vector3D::Normalize() {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		X *= invLength;
		Y *= invLength;
		Z *= invLength;
	}
}

// Easier access
float Vector3D::Dot(const Vector3D& other) const { return X * other.X + Y * other.Y + Z * other.Z; }

// Static functions
float Vector3D::Dot(const Vector3D& a, const Vector3D& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }

Vector3D Vector3D::Cross(const Vector3D& a, const Vector3D& b) {
	return Vector3D(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
}

float Vector3D::Distance(const Vector3D& a, const Vector3D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	float dz = a.Z - b.Z;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

float Vector3D::SquareDistance(const Vector3D& a, const Vector3D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	float dz = a.Z - b.Z;
	return dx * dx + dy * dy + dz * dz;
}

Vector3D Vector3D::Lerp(const Vector3D& a, const Vector3D& b, float t) {
	// lerp = a + (b - a) t; clamp t between 0 and 1
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector3D(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t);
}

Vector3D Vector3D::ClampVector(const Vector3D& vector, const Vector3D& min, const Vector3D& max) {
	// Clamp each component individually
	float clampedX = (vector.X < min.X) ? min.X : ((vector.X > max.X) ? max.X : vector.X);
	float clampedY = (vector.Y < min.Y) ? min.Y : ((vector.Y > max.Y) ? max.Y : vector.Y);
	float clampedZ = (vector.Z < min.Z) ? min.Z : ((vector.Z > max.Z) ? max.Z : vector.Z);
	return Vector3D(clampedX, clampedY, clampedZ);
}

float Vector3D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// Binary operators (non-member functions)
Vector3D operator+(const Vector3D& a, const Vector3D& b) { return Vector3D(a.X + b.X, a.Y + b.Y, a.Z + b.Z); }
Vector3D operator-(const Vector3D& a, const Vector3D& b) { return Vector3D(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }
Vector3D operator*(const Vector3D& vector, float scalar) { return Vector3D(vector.X * scalar, vector.Y * scalar, vector.Z * scalar); }
Vector3D operator*(float scalar, const Vector3D& vector) { return Vector3D(vector.X * scalar, vector.Y * scalar, vector.Z * scalar); }

Vector3D operator/(const Vector3D& vector, float scalar) { 
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector3D(vector.X * invScalar, vector.Y * invScalar, vector.Z * invScalar);
}

bool operator==(const Vector3D& a, const Vector3D& b) { return (a.X == b.X) && (a.Y == b.Y) && (a.Z == b.Z); }  
bool operator!=(const Vector3D& a, const Vector3D& b) { return !(a == b); }
