/* Start Header *****************************************************************/
/*!
\file   Vector3D.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
3D vector math implementation. Covers construction, arithmetic operators,
length and normalization, dot and cross products, distance queries,
interpolation and clamping utilities, and static direction constants.
*/
/* End Header *******************************************************************/

#include "math/Vector3D.h"
#include <cmath>
#include <stdexcept>

// ============================================================================
// Constructors
// ============================================================================

// Default-construct to the origin
Vector3D::Vector3D() : X(0.0f), Y(0.0f), Z(0.0f) {}

// Construct from explicit components
Vector3D::Vector3D(float x, float y, float z) : X(x), Y(y), Z(z) {}

// ============================================================================
// Assignment operators (member functions)
// ============================================================================

// Element-wise addition-assign
Vector3D& Vector3D::operator+=(const Vector3D& other) {
	X += other.X;
	Y += other.Y;
	Z += other.Z;
	return *this;
}

// Element-wise subtraction-assign
Vector3D& Vector3D::operator-=(const Vector3D& other) {
	X -= other.X;
	Y -= other.Y;
	Z -= other.Z;
	return *this;
}

// Uniform scalar multiplication-assign
Vector3D& Vector3D::operator*=(float scalar) {
	X *= scalar;
	Y *= scalar;
	Z *= scalar;
	return *this;
}

// Uniform scalar division-assign; multiply by reciprocal to avoid dividing three times
Vector3D& Vector3D::operator/=(float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");

	// Multiply by reciprocal rather than dividing three times
	float invScalar = 1.0f / scalar;
	X *= invScalar;
	Y *= invScalar;
	Z *= invScalar;
	return *this;
}

// ============================================================================
// Unary operator
// ============================================================================

// Return a negated copy; does not modify this vector
Vector3D Vector3D::operator-() const { return Vector3D(-X, -Y, -Z); }

// ============================================================================
// Member functions
// ============================================================================

// Euclidean length via Pythagoras; use SquareLength for comparisons to avoid the sqrt
float Vector3D::Length() const { return sqrtf(X * X + Y * Y + Z * Z); }

// Squared length; cheaper than Length() and sufficient for magnitude comparisons
float Vector3D::SquareLength() const { return X * X + Y * Y + Z * Z; }

// Return a unit-length copy without modifying this vector;
// returns zero vector if this vector has no length to avoid a divide-by-zero
Vector3D Vector3D::Normalized() const {
	float length = Length();

	if (length > 0) {
		float invLength = 1.0f / length;
		return Vector3D(X * invLength, Y * invLength, Z * invLength);
	}

	// Zero-length vector has no defined direction; return zero rather than NaN
	return Vector3D(0.0f, 0.0f, 0.0f);
}

// Normalize this vector in place; no-op if already zero to avoid a divide-by-zero
void Vector3D::Normalize() {
	float length = Length();

	if (length > 0) {
		float invLength = 1.0f / length;
		X *= invLength;
		Y *= invLength;
		Z *= invLength;
	}
}

// Instance dot product; provided so physics/lighting code can call dot on a vector directly
float Vector3D::Dot(const Vector3D& other) const { return X * other.X + Y * other.Y + Z * other.Z; }

// ============================================================================
// Static functions
// ============================================================================

// Dot product: |a||b|cos(theta); positive means same hemisphere, negative means opposed
float Vector3D::Dot(const Vector3D& a, const Vector3D& b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }

// Cross product: produces a vector orthogonal to both a and b following the right-hand rule;
// magnitude equals |a||b|sin(theta), so it is zero when the vectors are parallel
Vector3D Vector3D::Cross(const Vector3D& a, const Vector3D& b) {
	return Vector3D(
		a.Y * b.Z - a.Z * b.Y,   // X: cofactor from YZ plane
		a.Z * b.X - a.X * b.Z,   // Y: cofactor from ZX plane
		a.X * b.Y - a.Y * b.X    // Z: cofactor from XY plane
	);
}

// Euclidean distance between two points
float Vector3D::Distance(const Vector3D& a, const Vector3D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	float dz = a.Z - b.Z;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

// Squared distance; avoids sqrt and is sufficient when only relative distances matter
float Vector3D::SquareDistance(const Vector3D& a, const Vector3D& b) {
	float dx = a.X - b.X;
	float dy = a.Y - b.Y;
	float dz = a.Z - b.Z;
	return dx * dx + dy * dy + dz * dz;
}

// Linear interpolation: lerp(a, b, t) = a + (b - a) * t;
// t is clamped to [0, 1] so callers do not need to guard against overshooting
Vector3D Vector3D::Lerp(const Vector3D& a, const Vector3D& b, float t) {
	t = (t < 0) ? 0 : ((t > 1) ? 1 : t);
	return Vector3D(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t);
}

// Per-component clamp; each axis is clamped independently against its own min/max
Vector3D Vector3D::ClampVector(const Vector3D& vector, const Vector3D& min, const Vector3D& max) {
	float clampedX = (vector.X < min.X) ? min.X : ((vector.X > max.X) ? max.X : vector.X);
	float clampedY = (vector.Y < min.Y) ? min.Y : ((vector.Y > max.Y) ? max.Y : vector.Y);
	float clampedZ = (vector.Z < min.Z) ? min.Z : ((vector.Z > max.Z) ? max.Z : vector.Z);
	return Vector3D(clampedX, clampedY, clampedZ);
}

// Scalar clamp utility; convenience wrapper so callers do not branch manually
float Vector3D::ClampValue(float component, float min, float max) {
	if (component < min) return min;
	if (component > max) return max;
	return component;
}

// ============================================================================
// Binary operators (non-member functions)
// ============================================================================

// Element-wise addition
Vector3D operator+(const Vector3D& a, const Vector3D& b) { return Vector3D(a.X + b.X, a.Y + b.Y, a.Z + b.Z); }

// Element-wise subtraction
Vector3D operator-(const Vector3D& a, const Vector3D& b) { return Vector3D(a.X - b.X, a.Y - b.Y, a.Z - b.Z); }

// Both orderings provided so scalar * vector and vector * scalar compile equally
Vector3D operator*(const Vector3D& vector, float scalar) { return Vector3D(vector.X * scalar, vector.Y * scalar, vector.Z * scalar); }
Vector3D operator*(float scalar, const Vector3D& vector) { return Vector3D(vector.X * scalar, vector.Y * scalar, vector.Z * scalar); }

// Scalar division; non-member so the operand order matches vector / scalar naturally
Vector3D operator/(const Vector3D& vector, float scalar) {
	if (scalar == 0.0f) throw std::runtime_error("Division by zero");
	float invScalar = 1.0f / scalar;
	return Vector3D(vector.X * invScalar, vector.Y * invScalar, vector.Z * invScalar);
}

// Exact float comparison; callers should use distance checks for epsilon equality
bool operator==(const Vector3D& a, const Vector3D& b) { return (a.X == b.X) && (a.Y == b.Y) && (a.Z == b.Z); }

// Inequality via negated equality check
bool operator!=(const Vector3D& a, const Vector3D& b) { return !(a == b); }

// ============================================================================
// Static member definitions
// ============================================================================

const Vector3D Vector3D::Zero = Vector3D(0.0f, 0.0f, 0.0f);
const Vector3D Vector3D::Forward = Vector3D(0.0f, 0.0f, 1.0f);
const Vector3D Vector3D::Up = Vector3D(0.0f, 1.0f, 0.0f);
const Vector3D Vector3D::Right = Vector3D(1.0f, 0.0f, 0.0f);
const Vector3D Vector3D::Backward = Vector3D(0.0f, 0.0f, -1.0f);
const Vector3D Vector3D::Down = Vector3D(0.0f, -1.0f, 0.0f);
const Vector3D Vector3D::Left = Vector3D(-1.0f, 0.0f, 0.0f);
