/* Start Header *****************************************************************/
/*!
\file   Vector2D.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
2D vector math implementation. Covers construction, arithmetic operators,
length and normalization, dot and cross products, distance queries,
interpolation and clamping utilities, and static direction constants.
*/
/* End Header *******************************************************************/

#include "math/Vector2D.h"
#include <cmath>
#include <stdexcept>

// ============================================================================
// Constructors
// ============================================================================

// Default-construct to the origin
Vector2D::Vector2D() : X(0.0f), Y(0.0f) {}

// Construct from explicit components
Vector2D::Vector2D(float x, float y) : X(x), Y(y) {}

// ============================================================================
// Assignment operators (member functions)
// ============================================================================

// Element-wise addition-assign
Vector2D& Vector2D::operator+=(const Vector2D& other) {
	X += other.X;
	Y += other.Y;
	return *this;
}

// Element-wise subtraction-assign
Vector2D& Vector2D::operator-=(const Vector2D& other) {
	X -= other.X;
	Y -= other.Y;
	return *this;
}

// Uniform scalar multiplication-assign
Vector2D& Vector2D::operator*=(float scalar) {
	X *= scalar;
	Y *= scalar;
	return *this;
}

// Uniform scalar division-assign; multiply by reciprocal to avoid dividing twice
Vector2D& Vector2D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");

	// Multiply by reciprocal rather than dividing twice
	float invScalar = 1.0f / scalar;
	X *= invScalar;
	Y *= invScalar;
	return *this;
}

// ============================================================================
// Unary operator
// ============================================================================

// Return a negated copy; does not modify this vector
Vector2D Vector2D::operator-() const { return Vector2D(-X, -Y); }

// ============================================================================
// Member functions
// ============================================================================

// Euclidean length via Pythagoras; use SquareLength for comparisons to avoid the sqrt
float Vector2D::Length() const { return sqrtf(X * X + Y * Y); }

// Squared length; cheaper than Length() and sufficient for magnitude comparisons
float Vector2D::SquareLength() const { return X * X + Y * Y; }

// Return a unit-length copy without modifying this vector;
// returns zero vector if this vector has no length to avoid a divide-by-zero
Vector2D Vector2D::Normalized() const {
	float length = Length();

	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector2D(X * invLength, Y * invLength);
	}

	// Zero-length vector has no defined direction; return zero rather than NaN
	return Vector2D(0.0f, 0.0f);
}

// Normalize this vector in place; no-op if already zero to avoid a divide-by-zero
void Vector2D::Normalize() {
	float length = Length();

	if (length > 0) {
		float invLength = 1.0f / length;
		X *= invLength;
		Y *= invLength;
	}
}

// Instance dot product; provided so collision/physics code can call dot on a vector directly
float Vector2D::Dot(const Vector2D& other) const { return X * other.X + Y * other.Y; }

// ============================================================================
// Static functions
// ============================================================================

// Dot product: |a||b|cos(theta); positive means same hemisphere, negative means opposed
float Vector2D::Dot(const Vector2D& a, const Vector2D& b) { return a.X * b.X + a.Y * b.Y; }

// 2D cross product: scalar z-component of the 3D cross; positive if b is CCW from a
float Vector2D::Cross(const Vector2D& a, const Vector2D& b) { return a.X * b.Y - a.Y * b.X; }

// Euclidean distance between two points
float Vector2D::Distance(const Vector2D& a, const Vector2D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	return sqrtf(dx * dx + dy * dy);
}

// Squared distance; avoids sqrt and is sufficient when only relative distances matter
float Vector2D::SquareDistance(const Vector2D& a, const Vector2D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	return dx * dx + dy * dy;
}

// Linear interpolation: lerp(a, b, t) = a + (b - a) * t;
// t is clamped to [0, 1] so callers do not need to guard against overshooting
Vector2D Vector2D::Lerp(const Vector2D& a, const Vector2D& b, float t) {
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector2D(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t);
}

// Per-component clamp; each axis is clamped independently against its own min/max
Vector2D Vector2D::ClampVector(const Vector2D& vector, const Vector2D& min, const Vector2D& max) {
	float clampedX = (vector.X < min.X) ? min.X : ((vector.X > max.X) ? max.X : vector.X);
	float clampedY = (vector.Y < min.Y) ? min.Y : ((vector.Y > max.Y) ? max.Y : vector.Y);
	return Vector2D(clampedX, clampedY);
}

// Scalar clamp utility; convenience wrapper so callers do not branch manually
float Vector2D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

// Element-wise addition
Vector2D operator+(const Vector2D& a, const Vector2D& b) { return Vector2D(a.X + b.X, a.Y + b.Y); }

// Element-wise subtraction
Vector2D operator-(const Vector2D& a, const Vector2D& b) { return Vector2D(a.X - b.X, a.Y - b.Y); }

// Both orderings provided so scalar * vector and vector * scalar compile equally
Vector2D operator*(const Vector2D& vector, float scalar) { return Vector2D(vector.X * scalar, vector.Y * scalar); }
Vector2D operator*(float scalar, const Vector2D& vector) { return Vector2D(vector.X * scalar, vector.Y * scalar); }

// Scalar division; non-member so the operand order matches vector / scalar naturally
Vector2D operator/(const Vector2D& vector, float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector2D(vector.X * invScalar, vector.Y * invScalar);
}

// Exact float comparison; callers should use distance checks for epsilon equality
bool operator==(const Vector2D& a, const Vector2D& b) { return (a.X == b.X) && (a.Y == b.Y); }

// Inequality via negated equality check
bool operator!=(const Vector2D& a, const Vector2D& b) { return !(a == b); }

// ============================================================================
// Static member definitions
// ============================================================================

const Vector2D Vector2D::Zero = Vector2D(0.0f, 0.0f);
const Vector2D Vector2D::Up = Vector2D(0.0f, 1.0f);
const Vector2D Vector2D::Right = Vector2D(1.0f, 0.0f);
const Vector2D Vector2D::Down = Vector2D(0.0f, -1.0f);
const Vector2D Vector2D::Left = Vector2D(-1.0f, 0.0f);
