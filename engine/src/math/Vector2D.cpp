#include "Math/Vector2D.h"
#include <cmath> // For trigonometric or arithmetic operations
#include <stdexcept>

// Constructors
Vector2D::Vector2D() : X(0.0f), Y(0.0f) {}
Vector2D::Vector2D(float x, float y) : X(x), Y(y) {}

// Assignment operators (member functions)
Vector2D& Vector2D::operator+=(const Vector2D& other) {
	X += other.X;
	Y += other.Y;
	return *this;
}

Vector2D& Vector2D::operator-=(const Vector2D& other) {
	X -= other.X;
	Y -= other.Y;
	return *this;
}

Vector2D& Vector2D::operator*=(float scalar) {
	X *= scalar;
	Y *= scalar;
	return *this;
}

Vector2D& Vector2D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar; 
	X *= invScalar;
	Y *= invScalar;
	return *this;
}

// Unary operator
Vector2D Vector2D::operator-() const { return Vector2D(-X, -Y); }

// Member functions
float Vector2D::Length() const { return sqrtf(X * X + Y * Y); }
float Vector2D::SquareLength() const { return X * X + Y * Y; }

// Returns a new vector
Vector2D Vector2D::Normalized() const {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector2D(X * invLength, Y * invLength);
	}
	return Vector2D(0.0f, 0.0f);
}

// Modifies the current vector
void Vector2D::Normalize() {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		X *= invLength;
		Y *= invLength;
	}
}

// To fit with collision code
float Vector2D::Dot(const Vector2D& other) const { return X * other.X + Y * other.Y; }

// Static functions
float Vector2D::Dot(const Vector2D& a, const Vector2D& b) { return a.X * b.X + a.Y * b.Y; }
float Vector2D::Cross(const Vector2D& a, const Vector2D& b) { return a.X * b.Y - a.Y * b.X; }

float Vector2D::Distance(const Vector2D& a, const Vector2D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	return sqrtf(dx * dx + dy * dy);
}

float Vector2D::SquareDistance(const Vector2D& a, const Vector2D& b) { 
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	return dx * dx + dy * dy;
}

Vector2D Vector2D::Lerp(const Vector2D& a, const Vector2D& b, float t) {
	// lerp = a + (b - a) t; clamp t between 0 and 1
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector2D(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t);
}

Vector2D Vector2D::ClampVector(const Vector2D& vector, const Vector2D& min, const Vector2D& max) {
	// Clamp each component individually
	float clampedX = (vector.X < min.X) ? min.X : ((vector.X > max.X) ? max.X : vector.X);
	float clampedY = (vector.Y < min.Y) ? min.Y : ((vector.Y > max.Y) ? max.Y : vector.Y);
	return Vector2D(clampedX, clampedY);
}

float Vector2D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// Binary operators (non-member functions)
Vector2D operator+(const Vector2D& a, const Vector2D& b) { return Vector2D(a.X + b.X, a.Y + b.Y); }
Vector2D operator-(const Vector2D& a, const Vector2D& b) { return Vector2D(a.X - b.X, a.Y - b.Y); }
Vector2D operator*(const Vector2D& vector, float scalar) { return Vector2D(vector.X * scalar, vector.Y * scalar); }
Vector2D operator*(float scalar, const Vector2D& vector) { return Vector2D(vector.X * scalar, vector.Y * scalar); }

Vector2D operator/(const Vector2D& vector, float scalar) { 
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector2D(vector.X * invScalar, vector.Y * invScalar);
}

bool operator==(const Vector2D& a, const Vector2D& b) { return (a.X == b.X) && (a.Y == b.Y); }
bool operator!=(const Vector2D& a, const Vector2D& b) { return !(a == b); }
