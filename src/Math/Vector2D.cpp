#include "Math/Vector2D.h"
#include <cmath> // For trigonometric or arithmetic operations
#include <stdexcept>

// Constructors
Vector2D::Vector2D() : x(0.0f), y(0.0f) {}
Vector2D::Vector2D(float x, float y) : x(x), y(y) {}

// Assignment operators (member functions)
Vector2D& Vector2D::operator+=(const Vector2D& other) {
	x += other.x;
	y += other.y;
	return *this;
}

Vector2D& Vector2D::operator-=(const Vector2D& other) {
	x -= other.x;
	y -= other.y;
	return *this;
}

Vector2D& Vector2D::operator*=(float scalar) {
	x *= scalar;
	y *= scalar;
	return *this;
}

Vector2D& Vector2D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar; 
	x *= invScalar;
	y *= invScalar;
	return *this;
}

// Unary operator
Vector2D Vector2D::operator-() const { return Vector2D(-x, -y); }

// Member functions
float Vector2D::Length() const { return sqrtf(x * x + y * y); }
float Vector2D::SquareLength() const { return x * x + y * y; }

// Returns a new vector
Vector2D Vector2D::Normalized() const {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector2D(x * invLength, y * invLength);
	}
	return Vector2D(0.0f, 0.0f);
}

// Modifies the current vector
void Vector2D::Normalize() {
	float length = Length();
	if (length > 0) {
		float invLength = 1.0f / length;
		x *= invLength;
		y *= invLength;
	}
}

// To fit with collision code
float Vector2D::Dot(const Vector2D& other) const { return x * other.x + y * other.y; }

// Static functions
float Vector2D::Dot(const Vector2D& a, const Vector2D& b) { return a.x * b.x + a.y * b.y; }
float Vector2D::Cross(const Vector2D& a, const Vector2D& b) { return a.x * b.y - a.y * b.x; }

float Vector2D::Distance(const Vector2D& a, const Vector2D& b) {
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return sqrtf(dx * dx + dy * dy);
}

float Vector2D::SquareDistance(const Vector2D& a, const Vector2D& b) { 
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return dx * dx + dy * dy;
}

Vector2D Vector2D::Lerp(const Vector2D& a, const Vector2D& b, float t) {
	// lerp = a + (b - a) t; clamp t between 0 and 1
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector2D(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

Vector2D Vector2D::ClampVector(const Vector2D& vector, const Vector2D& min, const Vector2D& max) {
	// Clamp each component individually
	float clampedX = (vector.x < min.x) ? min.x : ((vector.x > max.x) ? max.x : vector.x);
	float clampedY = (vector.y < min.y) ? min.y : ((vector.y > max.y) ? max.y : vector.y);
	return Vector2D(clampedX, clampedY);
}

float Vector2D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// Binary operators (non-member functions)
Vector2D operator+(const Vector2D& a, const Vector2D& b) { return Vector2D(a.x + b.x, a.y + b.y); }
Vector2D operator-(const Vector2D& a, const Vector2D& b) { return Vector2D(a.x - b.x, a.y - b.y); }
Vector2D operator*(const Vector2D& vector, float scalar) { return Vector2D(vector.x * scalar, vector.y * scalar); }
Vector2D operator*(float scalar, const Vector2D& vector) { return Vector2D(vector.x * scalar, vector.y * scalar); }

Vector2D operator/(const Vector2D& vector, float scalar) { 
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector2D(vector.x * invScalar, vector.y * invScalar);
}

bool operator==(const Vector2D& a, const Vector2D& b) { return (a.x == b.x) && (a.y == b.y); }
bool operator!=(const Vector2D& a, const Vector2D& b) { return !(a == b); }
