#include "math/Vector4D.h"
#include <cmath>  // For trigonometric or arithmetic operations
#include <stdexcept>

// Constructors
Vector4D::Vector4D() : X(0.0f), Y(0.0f), Z(0.0f), W(0.0f) {}
Vector4D::Vector4D(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

// Assignment operators (member functions)
Vector4D& Vector4D::operator+=(const Vector4D& other) {
	X += other.X;
	Y += other.Y;
	Z += other.Z;
	W += other.W;
	return *this;
}

Vector4D& Vector4D::operator-=(const Vector4D& other) {
	X -= other.X;
	Y -= other.Y;
	Z -= other.Z;
	W -= other.W;
	return *this;
}

Vector4D& Vector4D::operator*=(float scalar) {
	X *= scalar;
	Y *= scalar;
	Z *= scalar;
	W *= scalar;
	return *this;
}

Vector4D& Vector4D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	X *= invScalar;
	Y *= invScalar;
	Z *= invScalar;
	W *= invScalar;
	return *this;
}

// Unary operator
Vector4D Vector4D::operator-() const { return Vector4D(-X, -Y, -Z, -W); }

// Member functions
float Vector4D::Length() const { return sqrtf(X * X + Y * Y + Z * Z + W * W); }
float Vector4D::SquareLength() const { return X * X + Y * Y + Z * Z + W * W; }

// Returns a new vector
Vector4D Vector4D::Normalized() const {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector4D(X * invLength, Y * invLength, Z * invLength, W * invLength);
	}
	return Vector4D(0.0f, 0.0f, 0.0f, 0.0f);
}

// Modifies the current vector
void Vector4D::Normalize() {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		X *= invLength;
		Y *= invLength;
		Z *= invLength;
		W *= invLength;
	}
}

// Easier access
float Vector4D::Dot(const Vector4D& other) const { return X * other.X + Y * other.Y + Z * other.Z + W * other.W; }

// Static functions
float Vector4D::Dot(const Vector4D& a, const Vector4D& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W; }

float Vector4D::Distance(const Vector4D& a, const Vector4D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	float dz = a.Z - b.Z;
	float dw = a.W - b.W;
	return sqrtf(dx * dx + dy * dy + dz * dz + dw * dw);
}

float Vector4D::SquareDistance(const Vector4D& a, const Vector4D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	float dz = a.Z - b.Z;
	float dw = a.W - b.W;
	return dx * dx + dy * dy + dz * dz + dw * dw;
}

Vector4D Vector4D::Lerp(const Vector4D& a, const Vector4D& b, float t) {
	// lerp = a + (b - a) t; clamp t between 0 and 1
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector4D(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t, a.W + (b.W - a.W) * t);
}

Vector4D Vector4D::Homogenize(const Vector4D& vector) { 
	if (vector.W != 0.0f) {
		float invW = 1.0f / vector.W;
		return Vector4D(
			vector.X * invW,
			vector.Y * invW,
			vector.Z * invW,
			1.0f
		);
	}
	return vector;
}

Vector4D Vector4D::ClampVector(const Vector4D& vector, const Vector4D& min, const Vector4D& max) {
	// Clamp each component individually
	float clampedX = (vector.X < min.X) ? min.X : ((vector.X > max.X) ? max.X : vector.X);
	float clampedY = (vector.Y < min.Y) ? min.Y : ((vector.Y > max.Y) ? max.Y : vector.Y);
	float clampedZ = (vector.Z < min.Z) ? min.Z : ((vector.Z > max.Z) ? max.Z : vector.Z);
	float clampedW = (vector.W < min.W) ? min.W : ((vector.W > max.W) ? max.W : vector.W);
	return Vector4D(clampedX, clampedY, clampedZ, clampedW);
}

float Vector4D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// If W == 1.0f
bool Vector4D::IsPoint() const { return W == 1.0f; }
// If W == 0.0f
bool Vector4D::IsVector() const { return W == 0.0f; }
// Extract 3D components
Vector3D Vector4D::XYZ() const { return Vector3D(X, Y, Z); }

// Binary operators (non-member functions)
Vector4D operator+(const Vector4D& a, const Vector4D& b) { return Vector4D(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W); }
Vector4D operator-(const Vector4D& a, const Vector4D& b) { return Vector4D(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W); }
Vector4D operator*(const Vector4D& vector, float scalar) { return Vector4D(vector.X * scalar, vector.Y * scalar, vector.Z * scalar, vector.W * scalar); }
Vector4D operator*(float scalar, const Vector4D& vector) { return Vector4D(vector.X * scalar, vector.Y * scalar, vector.Z * scalar, vector.W * scalar); }

Vector4D operator/(const Vector4D& vector, float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector4D(vector.X * invScalar, vector.Y * invScalar, vector.Z * invScalar, vector.W * invScalar);
}

bool operator==(const Vector4D& a, const Vector4D& b) { return (a.X == b.X) && (a.Y == b.Y) && (a.Z == b.Z) && (a.W == b.W); }
bool operator!=(const Vector4D& a, const Vector4D& b) { return !(a == b); }

// Static member definitions
const Vector4D Vector4D::Zero = Vector4D(0.0f, 0.0f, 0.0f, 0.0f);
